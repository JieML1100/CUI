#pragma once
#define NOMINMAX
#include "TextBox.h"
#include "Button.h"
#include "Window.h"
#include "TextEditCore.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <float.h>
#include <stdexcept>
#include <typeindex>

namespace
{
	CuiTextEdit::EditOptions SingleLineEditOptions()
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = false;
		return options;
	}

	std::wstring ApplyCharacterCasing(
		std::wstring value, CharacterCasing casing)
	{
		if (casing == CharacterCasing::Normal) return value;
		for (auto& ch : value)
		{
			ch = static_cast<wchar_t>(
				casing == CharacterCasing::Upper
					? std::towupper(ch)
					: std::towlower(ch));
		}
		return value;
	}

	DWRITE_TEXT_ALIGNMENT ToDirectWriteAlignment(
		TextAlignment value) noexcept
	{
		switch (value)
		{
		case TextAlignment::Right:
			return DWRITE_TEXT_ALIGNMENT_TRAILING;
		case TextAlignment::Center:
			return DWRITE_TEXT_ALIGNMENT_CENTER;
		case TextAlignment::Justify:
			return DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
		case TextAlignment::Left:
		default:
			return DWRITE_TEXT_ALIGNMENT_LEADING;
		}
	}

	DWRITE_WORD_WRAPPING ToDirectWriteWrapping(
		TextWrapping value) noexcept
	{
		switch (value)
		{
		case TextWrapping::Wrap:
			return DWRITE_WORD_WRAPPING_WRAP;
		case TextWrapping::WrapWithOverflow:
			return DWRITE_WORD_WRAPPING_WHOLE_WORD;
		case TextWrapping::NoWrap:
		default:
			return DWRITE_WORD_WRAPPING_NO_WRAP;
		}
	}

	void CommitTextChange(TextBox* control, const std::wstring& oldText, const std::wstring& newText)
	{
		if (!control || oldText == newText)
			return;
		// User edits are SetCurrentValue in WPF: update the effective target
		// without replacing a Local Binding or DynamicResource expression.
		(void)control->TrySetCurrentPropertyValue(
			TextBox::TextProperty(), BindingValue(newText));
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
				wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hData));
				if (pData)
				{
					memcpy(pData, text.c_str(), bytes);
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
}

UIClass TextBox::Type() { return UIClass::UI_TextBox; }

namespace
{
	DependencyPropertyOptions<TextBox, std::wstring> TextBoxTextOptions()
	{
		DependencyPropertyOptions<TextBox, std::wstring> options;
		options.DefaultValue = std::wstring{};
		// Match WPF TextBox.Text metadata: editing changes the text view, but it
		// does not invalidate measure.  In-place hosts such as DataGrid already
		// constrain the editor to the cell; propagating a measure request for
		// every character otherwise turns a text-only update into a complete
		// table/window layout pass and makes burst input visibly lag behind.
		options.Flags = DependencyPropertyFlags::AffectsRender
			| DependencyPropertyFlags::BindsTwoWayByDefault;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		options.DefaultUpdateMode = DataSourceUpdateMode::OnValidation;
		options.Changed = [](TextBox& target,
			const std::wstring& oldValue, const std::wstring& newValue)
		{
			TextChangedEventArgs args(oldValue, newValue);
			target.OnTextChanged(&target, args);
		};
		return options;
	}

	template<typename TValue>
	DependencyPropertyOptions<TextBox, TValue> TextBoxPropertyOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags =
			DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<TextBox, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Text";
		options.Design.CategoryOrder = 40;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	using DependencyPropertyAccessor = const DependencyProperty& (*)();

	auto TextBoxSubscriber(DependencyPropertyAccessor propertyAccessor)
	{
		return [propertyAccessor](
			TextBox& target,
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
}

const DependencyProperty& TextBox::TextProperty()
{
	static const auto registration = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		return DependencyPropertyRegistry::RegisterStatic<
			TextBox, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Text"),
				[](TextBox& target, Handler handler, DataSourceUpdateMode mode)
				{
					if (mode == DataSourceUpdateMode::OnValidation)
					{
						return target.OnLostFocus.Subscribe(
							[handler = std::move(handler)](Control*)
							{ handler(); });
					}
					return target.OnTextChanged.Subscribe(
						[handler = std::move(handler)](
							Control*, TextChangedEventArgs&) { handler(); });
				}, TextBoxTextOptions());
	}();
	return *registration;
}

const DependencyProperty& TextBox::MaxLengthProperty()
{
	static const auto registration = []
	{
		auto options = TextBoxPropertyOptions(
			0 CUI_DESIGN_METADATA_ARGUMENTS(
				10, DependencyPropertyEditorKind::Number),
			DependencyPropertyFlags::None);
		options.Validate = [](const int& value) { return value >= 0; };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		options.Design.Step = 1.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<TextBox, int>(
			DependencyPropertyRegistrationLiteral(L"MaxLength"),
			[](TextBox& target) { return target.MaxLength; },
			[](TextBox& target, const int& value)
			{ target.MaxLength = value; },
			TextBoxSubscriber(&TextBox::MaxLengthProperty), std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextBox::MinLinesProperty()
{
	static const auto registration = []
	{
		auto options = TextBoxPropertyOptions(
			1 CUI_DESIGN_METADATA_ARGUMENTS(
				20, DependencyPropertyEditorKind::Number),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		options.Validate = [](const int& value) { return value > 0; };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 1.0;
		options.Design.Step = 1.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<TextBox, int>(
			DependencyPropertyRegistrationLiteral(L"MinLines"),
			[](TextBox& target) { return target.MinLines; },
			[](TextBox& target, const int& value)
			{ target.MinLines = value; },
			TextBoxSubscriber(&TextBox::MinLinesProperty), std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextBox::MaxLinesProperty()
{
	static const auto registration = []
	{
		auto options = TextBoxPropertyOptions(
			(std::numeric_limits<int>::max)()
			CUI_DESIGN_METADATA_ARGUMENTS(
				30, DependencyPropertyEditorKind::Number),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		options.Validate = [](const int& value) { return value > 0; };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 1.0;
		options.Design.Step = 1.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<TextBox, int>(
			DependencyPropertyRegistrationLiteral(L"MaxLines"),
			[](TextBox& target) { return target.MaxLines; },
			[](TextBox& target, const int& value)
			{ target.MaxLines = value; },
			TextBoxSubscriber(&TextBox::MaxLinesProperty), std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextBox::TextAlignmentProperty()
{
	return TextAlignmentMetadataRelation().Property();
}

void TextBox::VisitDeclaredInheritedProperties(
	void* context, InheritedPropertyVisitor visitor) const
{
	TextBoxBase::VisitDeclaredInheritedProperties(context, visitor);
	if (visitor) visitor(context, TextAlignmentProperty());
}

const DependencyProperty& TextBox::TextWrappingProperty()
{
	return TextWrappingMetadataRelation().Property();
}

const DependencyPropertyMetadataRegistration&
TextBox::TextAlignmentMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		auto options = TextBoxPropertyOptions(
			::TextAlignment::Left
			CUI_DESIGN_METADATA_ARGUMENTS(
				40, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::Inherits
				| DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Left", BindingValue(::TextAlignment::Left) },
			{ L"Right", BindingValue(::TextAlignment::Right) },
			{ L"Center", BindingValue(::TextAlignment::Center) },
			{ L"Justify", BindingValue(::TextAlignment::Justify) }
		};
		)
		return DependencyPropertyRegistry::AddOwnerStatic<
			TextBox, ::TextAlignment>(
				Label::TextAlignmentProperty(),
				[](TextBox& target) { return target.TextAlignment; },
				[](TextBox& target, const ::TextAlignment& value)
				{ target.TextAlignment = value; },
				[](TextBox& target, Handler handler, DataSourceUpdateMode)
				{
					return target.OnPropertyValueChanged.Subscribe(
						[handler = std::move(handler)](
							DependencyObject*,
							const DependencyPropertyChangedEventArgs& args)
						{
							if (args.Property ==
								&TextBox::TextAlignmentProperty())
								handler();
						});
				}, std::move(options));
	}();
	return relation;
}

const DependencyPropertyMetadataRegistration&
TextBox::TextWrappingMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		auto options = TextBoxPropertyOptions(
			::TextWrapping::NoWrap
			CUI_DESIGN_METADATA_ARGUMENTS(
				50, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"NoWrap", BindingValue(::TextWrapping::NoWrap) },
			{ L"Wrap", BindingValue(::TextWrapping::Wrap) },
			{ L"WrapWithOverflow",
				BindingValue(::TextWrapping::WrapWithOverflow) }
		};
		)
		return DependencyPropertyRegistry::AddOwnerStatic<
			TextBox, ::TextWrapping>(
				Label::TextWrappingProperty(),
				[](TextBox& target) { return target.TextWrapping; },
				[](TextBox& target, const ::TextWrapping& value)
				{ target.TextWrapping = value; },
				[](TextBox& target, Handler handler, DataSourceUpdateMode)
				{
					return target.OnPropertyValueChanged.Subscribe(
						[handler = std::move(handler)](
							DependencyObject*,
							const DependencyPropertyChangedEventArgs& args)
						{
							if (args.Property ==
								&TextBox::TextWrappingProperty())
								handler();
						});
				}, std::move(options));
	}();
	return relation;
}

const DependencyProperty& TextBox::CharacterCasingProperty()
{
	static const auto registration = []
	{
		auto options = TextBoxPropertyOptions(
			::CharacterCasing::Normal CUI_DESIGN_METADATA_ARGUMENTS(
				60, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::None);
		options.Validate = [](const ::CharacterCasing& value)
		{
			return value == ::CharacterCasing::Normal
				|| value == ::CharacterCasing::Lower
				|| value == ::CharacterCasing::Upper;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Normal", BindingValue(::CharacterCasing::Normal) },
			{ L"Lower", BindingValue(::CharacterCasing::Lower) },
			{ L"Upper", BindingValue(::CharacterCasing::Upper) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextBox, ::CharacterCasing>(
				DependencyPropertyRegistrationLiteral(L"CharacterCasing"),
				[](TextBox& target) { return target.CharacterCasing; },
				[](TextBox& target, const ::CharacterCasing& value)
				{ target.CharacterCasing = value; },
				TextBoxSubscriber(&TextBox::CharacterCasingProperty),
				std::move(options));
	}();
	return *registration;
}

void TextBox::RegisterDependencyProperties()
{
	TextBoxBase::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)TextProperty();
	(void)MaxLengthProperty();
	(void)MinLinesProperty();
	(void)MaxLinesProperty();
	(void)CharacterCasingProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)TextAlignmentMetadataRelation();
	(void)TextWrappingMetadataRelation();
	(void)RegisterControlBorderThicknessMetadata<
		TextBox, TextBoxBase>(
			1.0f CUI_DESIGN_METADATA_ARGUMENTS(60));
	)
}

const DependencyPropertyMetadata*
TextBox::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &Label::TextAlignmentProperty())
		return &TextAlignmentMetadataRelation().Metadata();
	if (&property == &Label::TextWrappingProperty())
		return &TextWrappingMetadataRelation().Metadata();
	if (&property == &Control::BorderThicknessProperty())
	{
		return &RegisterControlBorderThicknessMetadata<
			TextBox, TextBoxBase>(
				1.0f CUI_DESIGN_METADATA_ARGUMENTS(60)).Metadata();
	}
	return TextBoxBase::ResolveExactDependencyPropertyMetadata(property);
}

GET_CPP(TextBox, std::wstring, Text)
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

std::wstring TextBox::GetSemanticText() const
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

SET_CPP(TextBox, std::wstring, Text)
{
	const auto previous = GetText();
	(void)SetDependencyPropertyValue(TextProperty(), std::move(value));
	if (GetText() == previous) return;
	_selectionStart = (std::clamp)(
		_selectionStart, 0, static_cast<int>(GetText().size()));
	_selectionEnd = (std::clamp)(
		_selectionEnd, 0, static_cast<int>(GetText().size()));
	InvalidateTextLayout();
	UpdateClearButtonPresentation();
	NotifySelectionChanged();
}

GET_CPP(TextBox, int, MaxLength)
{
	return _maxLength;
}
SET_CPP(TextBox, int, MaxLength)
{
	(void)SetPropertyField(MaxLengthProperty(), _maxLength, value);
}
GET_CPP(TextBox, int, MinLines)
{
	return _minLines;
}
SET_CPP(TextBox, int, MinLines)
{
	if (!SetPropertyField(MinLinesProperty(), _minLines, value)) return;
	UpdateLineConstraintPresentation();
}
GET_CPP(TextBox, int, MaxLines)
{
	return _maxLines;
}
SET_CPP(TextBox, int, MaxLines)
{
	if (!SetPropertyField(MaxLinesProperty(), _maxLines, value)) return;
	UpdateLineConstraintPresentation();
}
GET_CPP(TextBox, ::TextAlignment, TextAlignment)
{
	return _textAlignment;
}
SET_CPP(TextBox, ::TextAlignment, TextAlignment)
{
	if (!SetPropertyField(
		TextAlignmentProperty(), _textAlignment, value)) return;
	InvalidateTextLayout();
}
GET_CPP(TextBox, ::TextWrapping, TextWrapping)
{
	return _textWrapping;
}
SET_CPP(TextBox, ::TextWrapping, TextWrapping)
{
	if (!SetPropertyField(
		TextWrappingProperty(), _textWrapping, value)) return;
	InvalidateTextLayout();
	UpdateScroll();
}
GET_CPP(TextBox, ::CharacterCasing, CharacterCasing)
{
	return _characterCasing;
}
SET_CPP(TextBox, ::CharacterCasing, CharacterCasing)
{
	(void)SetPropertyField(
		CharacterCasingProperty(), _characterCasing, value);
}
bool TextBox::HandlesNavigationKey(Key key) const
{
	if (key == Key::Tab) return _acceptsTab;
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
TextBox::TextBox()
{
	RegisterDependencyProperties();
}
void TextBox::InputText(std::wstring input)
{
	if (_isReadOnly) return;
	SelectionNotificationScope selectionNotification{ this };
	input = ApplyCharacterCasing(
		std::move(input), _characterCasing);
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto options = SingleLineEditOptions();
	options.allowMultiLine = _acceptsReturn;
	options.acceptsTab = _acceptsTab;
	options.maxTextLength = static_cast<size_t>(_maxLength);
	auto result = CuiTextEdit::ReplaceSelection(
		newText, this->_selectionStart, this->_selectionEnd,
		input, options);
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = result.insertedText;
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		StoreUndoRecord(std::move(rec));
		this->redoStack.clear();
	}
	CommitTextChange(this, oldStr, newText);
}
void TextBox::InputBack()
{
	if (_isReadOnly) return;
	SelectionNotificationScope selectionNotification{ this };
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto options = SingleLineEditOptions();
	options.allowMultiLine = _acceptsReturn;
	auto result = CuiTextEdit::Backspace(
		newText, this->_selectionStart, this->_selectionEnd, options);
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		StoreUndoRecord(std::move(rec));
		this->redoStack.clear();
	}
	CommitTextChange(this, oldStr, newText);
}
void TextBox::InputDelete()
{
	if (_isReadOnly) return;
	SelectionNotificationScope selectionNotification{ this };
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto options = SingleLineEditOptions();
	options.allowMultiLine = _acceptsReturn;
	auto result = CuiTextEdit::DeleteForward(
		newText, this->_selectionStart, this->_selectionEnd, options);
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		StoreUndoRecord(std::move(rec));
		this->redoStack.clear();
	}
	CommitTextChange(this, oldStr, newText);
}
void TextBox::ApplyUndoRecord(const UndoRecord& rec, bool isUndo)
{
	if (_isReadOnly) return;
	SelectionNotificationScope selectionNotification{ this };
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	this->isApplyingUndoRedo = true;

	int pos = std::clamp(rec.pos, 0, (int)newText.size());
	const std::wstring& removeText = isUndo ? rec.insertedText : rec.removedText;
	const std::wstring& insertText = isUndo ? rec.removedText : rec.insertedText;

	if (!removeText.empty() && pos <= (int)newText.size())
	{
		size_t removeLen = std::min(removeText.size(), newText.size() - (size_t)pos);
		newText.erase((size_t)pos, removeLen);
	}
	if (!insertText.empty())
	{
		newText.insert((size_t)pos, insertText);
	}
	if (!_acceptsReturn)
	{
		for (auto& ch : newText)
		{
			if (ch == L'\r' || ch == L'\n') ch = L' ';
		}
	}
	if (isUndo)
	{
		this->_selectionStart = rec.selStartBefore;
		this->_selectionEnd = rec.selEndBefore;
	}
	else
	{
		this->_selectionStart = rec.selStartAfter;
		this->_selectionEnd = rec.selEndAfter;
	}
	this->_selectionStart = std::clamp(this->_selectionStart, 0, (int)newText.size());
	this->_selectionEnd = std::clamp(this->_selectionEnd, 0, (int)newText.size());

	this->isApplyingUndoRedo = false;
	CommitTextChange(this, oldStr, newText);
}

void TextBox::StoreUndoRecord(UndoRecord record)
{
	if (!_isUndoEnabled || _undoLimit == 0) return;
	undoStack.push_back(std::move(record));
	if (_undoLimit > 0
		&& undoStack.size() > static_cast<size_t>(_undoLimit))
	{
		undoStack.erase(
			undoStack.begin(),
			undoStack.begin()
				+ static_cast<std::ptrdiff_t>(
					undoStack.size() - static_cast<size_t>(_undoLimit)));
	}
}

void TextBox::StoreRedoRecord(UndoRecord record)
{
	if (!_isUndoEnabled || _undoLimit == 0) return;
	redoStack.push_back(std::move(record));
	if (_undoLimit > 0
		&& redoStack.size() > static_cast<size_t>(_undoLimit))
	{
		redoStack.erase(
			redoStack.begin(),
			redoStack.begin()
				+ static_cast<std::ptrdiff_t>(
					redoStack.size() - static_cast<size_t>(_undoLimit)));
	}
}

void TextBox::OnUndoPolicyChanged()
{
	undoStack.clear();
	redoStack.clear();
}

void TextBox::OnScrollPolicyChanged()
{
	if (_horizontalScrollBarVisibility == ScrollBarVisibility::Disabled)
		_horizontalScrollOffset = 0.0f;
	if (_verticalScrollBarVisibility == ScrollBarVisibility::Disabled)
		_verticalScrollOffset = 0.0f;
	InvalidateTextLayout();
}

void TextBox::Undo()
{
	if (!CanUndo()) return;
	UndoRecord rec = this->undoStack.back();
	this->undoStack.pop_back();
	ApplyUndoRecord(rec, true);
	StoreRedoRecord(std::move(rec));
}
void TextBox::Redo()
{
	if (!CanRedo()) return;
	UndoRecord rec = this->redoStack.back();
	this->redoStack.pop_back();
	ApplyUndoRecord(rec, false);
	StoreUndoRecord(std::move(rec));
}

void TextBox::InvalidateTextLayout() noexcept
{
	_textLayout.Reset();
	_layoutText.clear();
	_layoutFont = nullptr;
	_layoutViewportWidth = -1.0f;
	_caretRectCacheValid = false;
}

float TextBox::TextViewportWidth() noexcept
{
	float width = (std::max)(
		0.0f, ActualWidth - Padding.Left - Padding.Right);
	auto* clearButton =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"DeleteButton"));
	if (clearButton
		&& clearButton->Visibility == Visibility::Visible)
	{
		const auto ownerOrigin = GetAbsoluteLocationDip();
		const auto buttonOrigin = clearButton->GetAbsoluteLocationDip();
		const float buttonLeft = static_cast<float>(
			buttonOrigin.x - ownerOrigin.x) - Padding.Left;
		if (buttonLeft > 0.0f)
			width = (std::min)(width, buttonLeft);
	}
	return width;
}

float TextBox::TextViewportHeight() noexcept
{
	return (std::max)(
		0.0f, ActualHeight - Padding.Top - Padding.Bottom);
}

IDWriteTextLayout* TextBox::EnsureTextLayout()
{
	auto* font = GetRenderFont();
	if (!font || !font->FontObject) return nullptr;
	const auto text = Text;
	const float viewportWidth = TextViewportWidth();
	if (_textLayout
		&& _layoutText == text
		&& _layoutFont == font->FontObject
		&& _layoutViewportWidth == viewportWidth
		&& _layoutTextAlignment == _textAlignment
		&& _layoutTextWrapping == _textWrapping)
	{
		return _textLayout.Get();
	}

	_textLayout.Reset();
	_layoutText = text;
	_layoutFont = font->FontObject;
	_layoutViewportWidth = viewportWidth;
	_layoutTextAlignment = _textAlignment;
	_layoutTextWrapping = _textWrapping;
	_textLayout.Attach(Factory::CreateStringLayout(
		text,
		(std::max)(1.0f, viewportWidth),
		FLT_MAX,
		font->FontObject));
	if (!_textLayout) return nullptr;
	_textLayout->SetTextAlignment(
		ToDirectWriteAlignment(_textAlignment));
	_textLayout->SetWordWrapping(
		ToDirectWriteWrapping(_textWrapping));
	_textSize = font->GetTextSize(_textLayout.Get());
	// Selection/caret preparation can run before a newly generated editor has
	// received its first arrange, when the viewport is still zero.  Once the
	// real viewport arrives, keep the previously requested scroll position but
	// clamp it against the new extent.  Otherwise SelectAll/F2 can leave the
	// entire initial value shifted outside the first visible edit frame until
	// the next pointer or key report calls UpdateScroll again.
	_horizontalScrollOffset = (std::clamp)(
		_horizontalScrollOffset, 0.0f,
		(std::max)(0.0f, _textSize.width - viewportWidth));
	_verticalScrollOffset = (std::clamp)(
		_verticalScrollOffset, 0.0f,
		(std::max)(0.0f, _textSize.height - TextViewportHeight()));
	return _textLayout.Get();
}

float TextBox::GetTextOriginY(IDWriteTextLayout* layout)
{
	if (!layout) return Padding.Top;
	auto* font = GetRenderFont();
	const auto size = font
		? font->GetTextSize(layout) : D2D1::SizeF();
	const float available = TextViewportHeight();
	const float remaining = (std::max)(
		0.0f, available - size.height);
	switch (VerticalContentAlignment)
	{
	case VerticalAlignment::Bottom:
		return Padding.Top + remaining;
	case VerticalAlignment::Center:
		return Padding.Top + remaining * 0.5f;
	case VerticalAlignment::Stretch:
	case VerticalAlignment::Top:
	default:
		return Padding.Top;
	}
}

bool TextBox::GetCaretLayoutMetrics(
	int caretIndex, DWRITE_HIT_TEST_METRICS& metrics)
{
	auto* layout = EnsureTextLayout();
	if (!layout) return false;
	caretIndex = (std::clamp)(
		caretIndex, 0, static_cast<int>(Text.size()));
	FLOAT x = 0.0f;
	FLOAT y = 0.0f;
	return SUCCEEDED(layout->HitTestTextPosition(
		static_cast<UINT32>(caretIndex),
		FALSE, &x, &y, &metrics));
}

int TextBox::HitTestTextPosition(float localX, float localY)
{
	auto* layout = EnsureTextLayout();
	if (!layout) return 0;
	BOOL trailing = FALSE;
	BOOL inside = FALSE;
	DWRITE_HIT_TEST_METRICS metrics{};
	const float x = localX - Padding.Left
		+ _horizontalScrollOffset;
	const float y = localY - GetTextOriginY(layout)
		+ _verticalScrollOffset;
	if (FAILED(layout->HitTestPoint(
		x, y, &trailing, &inside, &metrics)))
	{
		return 0;
	}
	int result = static_cast<int>(
		metrics.textPosition
		+ (trailing ? metrics.length : 0));
	return (std::clamp)(
		result, 0, static_cast<int>(Text.size()));
}

int TextBox::GetVisualLineBoundary(bool lineEnd)
{
	auto* layout = EnsureTextLayout();
	if (!layout) return lineEnd
		? static_cast<int>(Text.size()) : 0;
	DWRITE_HIT_TEST_METRICS caret{};
	if (!GetCaretLayoutMetrics(_selectionEnd, caret))
		return _selectionEnd;
	BOOL trailing = FALSE;
	BOOL inside = FALSE;
	DWRITE_HIT_TEST_METRICS boundary{};
	const float x = lineEnd
		? (std::max)(1.0f, TextViewportWidth()) : 0.0f;
	if (FAILED(layout->HitTestPoint(
		x, caret.top + caret.height * 0.5f,
		&trailing, &inside, &boundary)))
	{
		return _selectionEnd;
	}
	int result = static_cast<int>(
		boundary.textPosition
		+ (lineEnd && trailing ? boundary.length : 0));
	return (std::clamp)(
		result, 0, static_cast<int>(Text.size()));
}

int TextBox::GetVerticalCaretIndex(float lineDelta)
{
	auto* layout = EnsureTextLayout();
	if (!layout) return _selectionEnd;
	DWRITE_HIT_TEST_METRICS caret{};
	if (!GetCaretLayoutMetrics(_selectionEnd, caret))
		return _selectionEnd;
	BOOL trailing = FALSE;
	BOOL inside = FALSE;
	DWRITE_HIT_TEST_METRICS target{};
	const float lineHeight = caret.height > 0.0f
		? caret.height
		: (GetRenderFont() ? GetRenderFont()->FontHeight : 16.0f);
	if (FAILED(layout->HitTestPoint(
		caret.left,
		caret.top + lineHeight * lineDelta
			+ lineHeight * 0.5f,
		&trailing, &inside, &target)))
	{
		return _selectionEnd;
	}
	const int result = static_cast<int>(
		target.textPosition
		+ (trailing ? target.length : 0));
	return (std::clamp)(
		result, 0, static_cast<int>(Text.size()));
}

void TextBox::UpdateScroll(bool arrival)
{
	(void)arrival;
	auto* layout = EnsureTextLayout();
	if (!layout) return;
	DWRITE_HIT_TEST_METRICS caret{};
	if (!GetCaretLayoutMetrics(_selectionEnd, caret)) return;

	const float viewportWidth = TextViewportWidth();
	const float viewportHeight = TextViewportHeight();
	const bool horizontalEnabled =
		_horizontalScrollBarVisibility
			!= ScrollBarVisibility::Disabled
		&& _textWrapping == TextWrapping::NoWrap;
	const bool verticalEnabled =
		_verticalScrollBarVisibility
			!= ScrollBarVisibility::Disabled;
	if (horizontalEnabled)
	{
		if (caret.left - _horizontalScrollOffset < 0.0f)
			_horizontalScrollOffset = caret.left;
		else if (caret.left + caret.width - _horizontalScrollOffset
			> viewportWidth)
		{
			_horizontalScrollOffset =
				caret.left + caret.width - viewportWidth;
		}
		const float maxOffset = (std::max)(
			0.0f, _textSize.width - viewportWidth);
		_horizontalScrollOffset = (std::clamp)(
			_horizontalScrollOffset, 0.0f, maxOffset);
	}
	else
	{
		_horizontalScrollOffset = 0.0f;
	}

	if (verticalEnabled)
	{
		if (caret.top - _verticalScrollOffset < 0.0f)
			_verticalScrollOffset = caret.top;
		else if (caret.top + caret.height - _verticalScrollOffset
			> viewportHeight)
		{
			_verticalScrollOffset =
				caret.top + caret.height - viewportHeight;
		}
		const float maxOffset = (std::max)(
			0.0f, _textSize.height - viewportHeight);
		_verticalScrollOffset = (std::clamp)(
			_verticalScrollOffset, 0.0f, maxOffset);
	}
	else
	{
		_verticalScrollOffset = 0.0f;
	}
}

void TextBox::NotifySelectionChanged()
{
	if (_lastNotifiedSelectionStart == _selectionStart
		&& _lastNotifiedSelectionEnd == _selectionEnd)
	{
		return;
	}
	const int oldStart = _lastNotifiedSelectionStart;
	_lastNotifiedSelectionStart = _selectionStart;
	_lastNotifiedSelectionEnd = _selectionEnd;
	SelectionChangedEventArgs args(oldStart, _selectionStart);
	SelectionChanged(this, args);
}
std::wstring TextBox::GetSelectedString()
{
	auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Text.size());
	if (!span.HasSelection())
		return L"";
	return this->Text.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}

// ---- 公共选择/编辑 API ----
int TextBox::GetSelectionStart()
{
	auto span = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, Text.size());
	return span.start;
}

int TextBox::GetSelectionLength()
{
	auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Text.size());
	return span.HasSelection() ? static_cast<int>(span.Length()) : 0;
}

int TextBox::GetCaretIndex()
{
	return (std::clamp)(
		_selectionEnd, 0, static_cast<int>(Text.size()));
}

void TextBox::SetCaretIndex(int value)
{
	Select(value, 0);
}

bool TextBox::HasSelection()
{
	return GetSelectionLength() > 0;
}

void TextBox::Select(int start, int length)
{
	SelectionNotificationScope selectionNotification{ this };
	const int textLen = static_cast<int>(this->Text.size());
	start = (std::clamp)(start, 0, textLen);
	length = (std::clamp)(length, 0, textLen - start);
	this->_selectionStart = start;
	this->_selectionEnd = start + length;
	UpdateScroll();
	this->InvalidateVisual();
}

int TextBox::GetCharacterIndexFromPoint(float localX, float localY)
{
	return HitTestTextPosition(localX, localY);
}

void TextBox::SelectAll()
{
	SelectionNotificationScope selectionNotification{ this };
	this->_selectionStart = 0;
	this->_selectionEnd = static_cast<int>(this->Text.size());
	UpdateScroll();
	this->InvalidateVisual();
}

void TextBox::ClearSelection()
{
	SelectionNotificationScope selectionNotification{ this };
	this->_selectionEnd = this->_selectionStart;
	UpdateScroll();
	this->InvalidateVisual();
}

void TextBox::Clear()
{
	if (_isReadOnly) return;
	this->SelectAll();
	this->InputBack();
}

void TextBox::InsertText(const std::wstring& text)
{
	if (_isReadOnly || (text.empty() && !HasSelection())) return;
	this->InputText(text);
}

bool TextBox::Copy()
{
	const std::wstring selected = this->GetSelectedString();
	if (selected.empty()) return false;
	return WriteClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, selected);
}

bool TextBox::Cut()
{
	if (_isReadOnly) return false;
	const std::wstring selected = this->GetSelectedString();
	if (selected.empty()) return false;
	if (!WriteClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, selected))
		return false;
	this->InputBack();
	return true;
}

bool TextBox::Paste()
{
	if (_isReadOnly) return false;
	std::wstring clipboardText;
	if (!TryReadClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, clipboardText))
		return false;
	if (clipboardText.empty()) return false;
	this->InputText(clipboardText);
	return true;
}

bool TextBox::ShouldHitTestChildrenAt(
	int localX, int localY) const
{
	const auto* clearButton =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"DeleteButton"));
	if (!clearButton
		|| !clearButton->GetIsVisible())
	{
		return false;
	}
	const auto ownerOrigin = GetAbsoluteLocationDip();
	const auto buttonOrigin = clearButton->GetAbsoluteLocationDip();
	const auto buttonSize = clearButton->GetActualSizeDip();
	const float x = static_cast<float>(ownerOrigin.x + localX);
	const float y = static_cast<float>(ownerOrigin.y + localY);
	return x >= static_cast<float>(buttonOrigin.x)
		&& y >= static_cast<float>(buttonOrigin.y)
		&& x < static_cast<float>(buttonOrigin.x) + buttonSize.width
		&& y < static_cast<float>(buttonOrigin.y) + buttonSize.height;
}

void TextBox::UpdateClearButtonPresentation()
{
	auto* clearButton =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"DeleteButton"));
	if (!clearButton) return;
	const auto visibility =
		!Text.empty() && IsEnabled && !_isReadOnly
			? Visibility::Visible
			: Visibility::Collapsed;
	if (clearButton->Visibility != visibility)
		clearButton->Visibility = visibility;
	InvalidateTextLayout();
}

void TextBox::UpdateLineConstraintPresentation()
{
	auto* contentHost =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_ContentHost"));
	auto* font = GetRenderFont();
	if (!contentHost || !font) return;
	const float lineHeight = (std::max)(
		1.0f, font->FontHeight);
	const float minHeight =
		lineHeight * static_cast<float>(_minLines);
	const int effectiveMaxLines =
		(std::max)(_minLines, _maxLines);
	const float maxHeight =
		effectiveMaxLines == (std::numeric_limits<int>::max)()
			? FLT_MAX
			: lineHeight * static_cast<float>(effectiveMaxLines);
	if (contentHost->MinHeight != minHeight)
		contentHost->MinHeight = minHeight;
	if (contentHost->MaxHeight != maxHeight)
		contentHost->MaxHeight = maxHeight;
}

void TextBox::OnControlTemplatePresentationChanged()
{
	ClearTemplatePartEventConnections();
	TextBoxBase::OnControlTemplatePresentationChanged();
	if (auto* clearButton = dynamic_cast<Button*>(
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"DeleteButton"))))
	{
		const ControlWeakReference lifetime(this);
		RetainTemplatePartEventConnection(
			clearButton->Click.Subscribe(
				[lifetime](Control*, RoutedEventArgs&)
				{
					auto* textBox =
						dynamic_cast<TextBox*>(lifetime.Get());
					if (!textBox || textBox->_isReadOnly) return;
					textBox->Clear();
					textBox->Focus();
				}));
	}
	UpdateClearButtonPresentation();
	UpdateLineConstraintPresentation();
}

void TextBox::PrepareMeasureCore(
	const cui::core::Constraints& available)
{
	(void)available;
	UpdateClearButtonPresentation();
	UpdateLineConstraintPresentation();
}

void TextBox::OnRender()
{
	if (!IsVisible) return;
	auto d2d = this->GetDrawingContext();
	auto font = this->GetRenderFont();
	if (!d2d || !font) return;
	auto* textLayout = EnsureTextLayout();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	Microsoft::WRL::ComPtr<ID2D1Brush> foreground;
	foreground.Attach(CreateForegroundBrush(
		*d2d, D2D1::SizeF(actualWidth, actualHeight)));
	auto selectionDefinition = this->SelectionBrush;
	selectionDefinition.Opacity *= static_cast<float>(
		(std::clamp)(this->SelectionOpacity, 0.0, 1.0));
	Microsoft::WRL::ComPtr<ID2D1Brush> selectionBrush;
	selectionBrush.Attach(selectionDefinition.CreateBrush(
		*d2d, D2D1::SizeF(actualWidth, actualHeight)));
	Microsoft::WRL::ComPtr<ID2D1Brush> selectionTextBrush;
	selectionTextBrush.Attach(this->SelectionTextBrush.CreateBrush(
		*d2d, D2D1::SizeF(actualWidth, actualHeight)));
	Microsoft::WRL::ComPtr<ID2D1Brush> caretBrush;
	caretBrush.Attach(this->CaretBrush.CreateBrush(
		*d2d, D2D1::SizeF(actualWidth, actualHeight)));
	const bool focused = GetPresentationWindow()
		&& GetPresentationWindow()->GetKeyboardFocusedElement() == this;
	const bool showSelection = focused
		|| IsInactiveSelectionHighlightEnabled;
	const bool showCaret = focused
		&& (!_isReadOnly || _isReadOnlyCaretVisible);
	this->_caretRectCacheValid = false;
	bool shouldDrawCaret = false;
	D2D1_POINT_2F caretStart{};
	D2D1_POINT_2F caretEnd{};
	const float textOriginX =
		Padding.Left - _horizontalScrollOffset;
	const float textOriginY =
		GetTextOriginY(textLayout) - _verticalScrollOffset;
	this->BeginRender();
	if (textLayout)
	{
		const auto span = CuiTextEdit::NormalizeSelection(
			_selectionStart, _selectionEnd, Text.size());
		if (showSelection && span.HasSelection() && selectionBrush)
		{
			auto ranges = font->HitTestTextRange(
				textLayout,
				static_cast<UINT32>(span.start),
				static_cast<UINT32>(span.Length()));
			for (const auto& range : ranges)
			{
				d2d->FillRect(
					range.left + textOriginX,
					range.top + textOriginY,
					range.width,
					range.height,
					selectionBrush.Get());
			}
		}

		if (showSelection && span.HasSelection()
			&& selectionTextBrush)
		{
			if (foreground)
				d2d->DrawStringLayoutEffect(
					textLayout, textOriginX, textOriginY,
					foreground.Get(),
					DWRITE_TEXT_RANGE{
						static_cast<UINT32>(span.start),
						static_cast<UINT32>(span.Length()) },
					selectionTextBrush.Get(), font);
			else
				d2d->DrawStringLayoutEffect(
					textLayout, textOriginX, textOriginY,
					RendererForegroundColor,
					DWRITE_TEXT_RANGE{
						static_cast<UINT32>(span.start),
						static_cast<UINT32>(span.Length()) },
					selectionTextBrush.Get(), font);
		}
		else if (foreground)
		{
			d2d->DrawStringLayout(
				textLayout, textOriginX, textOriginY,
				foreground.Get());
		}
		else
		{
			d2d->DrawStringLayout(
				textLayout, textOriginX, textOriginY,
				RendererForegroundColor);
		}

		if (showCaret && !span.HasSelection())
		{
			DWRITE_HIT_TEST_METRICS caret{};
			if (GetCaretLayoutMetrics(_selectionEnd, caret))
			{
				const float x = caret.left + textOriginX;
				const float y = caret.top + textOriginY;
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
	}

	UpdateCaretBlinkState(
		showCaret, _selectionStart, _selectionEnd,
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
	this->EndRender();
}

bool TextBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}

bool TextBox::ApplyTextInput(const TextCompositionEventArgs& input)
{
	if (_isReadOnly || input.Text.empty()) return false;
	InputText(input.Text);
	UpdateScroll();
	InvalidateVisual();
	return true;
}

bool TextBox::TryGetTextInputCaretRect(D2D1_RECT_F& outRect)
{
	if (_caretRectCacheValid)
	{
		outRect = TransformAbsoluteRectToRenderSpace(_caretRectCache);
		return true;
	}
	const auto absolute = GetAbsoluteLocationDip();
	DWRITE_HIT_TEST_METRICS caret{};
	if (!GetCaretLayoutMetrics(_selectionEnd, caret))
		return false;
	const float x = static_cast<float>(absolute.x)
		+ Padding.Left + caret.left - _horizontalScrollOffset;
	const float y = static_cast<float>(absolute.y)
		+ GetTextOriginY(EnsureTextLayout())
		+ caret.top - _verticalScrollOffset;
	const float height = caret.height > 0.0f
		? caret.height
		: (GetRenderFont() && GetRenderFont()->FontHeight > 0.0f
			? GetRenderFont()->FontHeight : 16.0f);
	outRect = TransformAbsoluteRectToRenderSpace(
		D2D1::RectF(x, y, x + 1.0f, y + height));
	return true;
}

bool TextBox::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;
	SelectionNotificationScope selectionNotification{ this };
	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
	{
		EnsureTextLayout();
		if (_verticalScrollBarVisibility
				!= ScrollBarVisibility::Disabled
			&& _textSize.height > TextViewportHeight())
		{
			const float line = GetRenderFont()
				? GetRenderFont()->FontHeight : 16.0f;
			const float maxOffset = (std::max)(
				0.0f, _textSize.height - TextViewportHeight());
			_verticalScrollOffset = (std::clamp)(
				_verticalScrollOffset
					- (input.WheelDelta > 0 ? line * 3.0f : -line * 3.0f),
				0.0f, maxOffset);
			InvalidateVisual();
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case InputReportKind::PointerMove:
	{
		if (input.IsButtonPressed(MouseButton::Left)
			&& this->GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		{
			_selectionEnd = HitTestTextPosition(
				static_cast<float>(input.X),
				static_cast<float>(input.Y));
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
			this->_selectionStart = this->_selectionEnd =
				HitTestTextPosition(
					static_cast<float>(input.X),
					static_cast<float>(input.Y));
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
			_selectionEnd = HitTestTextPosition(
				static_cast<float>(input.X),
				static_cast<float>(input.Y));
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
		const int hit = HitTestTextPosition(
			static_cast<float>(input.X),
			static_cast<float>(input.Y));
		const auto word = CuiTextEdit::GetWordSelectionSpan(
			Text, hit, _acceptsReturn);
		this->_selectionStart = word.start;
		this->_selectionEnd = word.end;
		UpdateScroll();
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDoubleClick(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::KeyDown:
	{
		bool handled = false;
		if (!_isReadOnly && input.Key == Key::Tab && _acceptsTab)
		{
			InputText(L"\t");
			UpdateScroll();
			InvalidateVisual();
			return true;
		}
		if (input.HasModifier(ModifierKeys::Control))
		{
			if (input.Key == Key::A)
			{
				SelectAll();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::C)
			{
				(void)Copy();
				return true;
			}
			if (!_isReadOnly && input.Key == Key::V)
			{
				(void)Paste();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (!_isReadOnly && input.Key == Key::X)
			{
				(void)Cut();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (!_isReadOnly && input.Key == Key::Z)
			{
				this->Undo();
				UpdateScroll();
				this->InvalidateVisual();
				return true;
			}
			if (!_isReadOnly && input.Key == Key::Y)
			{
				this->Redo();
				UpdateScroll();
				this->InvalidateVisual();
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
			int textLength = static_cast<int>(this->Text.size());
			const bool extendSelection = input.HasModifier(ModifierKeys::Shift);
			auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Text.size());
			if (!extendSelection && span.HasSelection())
			{
				this->_selectionStart = this->_selectionEnd = span.end;
				UpdateScroll();
			}
			else if (this->_selectionEnd < textLength)
			{
				this->_selectionEnd = CuiTextEdit::GetNextCaretIndex(this->Text, this->_selectionEnd, false);
				if (!extendSelection)
					this->_selectionStart = this->_selectionEnd;
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Left)
		{
			handled = true;
			const bool extendSelection = input.HasModifier(ModifierKeys::Shift);
			auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Text.size());
			if (!extendSelection && span.HasSelection())
			{
				this->_selectionStart = this->_selectionEnd = span.start;
				UpdateScroll();
			}
			else if (this->_selectionEnd > 0)
			{
				this->_selectionEnd = CuiTextEdit::GetPreviousCaretIndex(this->Text, this->_selectionEnd, false);
				if (!extendSelection)
					this->_selectionStart = this->_selectionEnd;
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Home)
		{
			handled = true;
			this->_selectionEnd =
				input.HasModifier(ModifierKeys::Control)
					? 0 : GetVisualLineBoundary(false);
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll();
		}
		else if (input.Key == Key::End)
		{
			handled = true;
			this->_selectionEnd =
				input.HasModifier(ModifierKeys::Control)
					? static_cast<int>(this->Text.size())
					: GetVisualLineBoundary(true);
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll();
		}
		else if (input.Key == Key::PageUp)
		{
			handled = true;
			const float lineHeight = GetRenderFont()
				? GetRenderFont()->FontHeight : 16.0f;
			this->_selectionEnd = GetVerticalCaretIndex(
				-TextViewportHeight() / (std::max)(1.0f, lineHeight));
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll(true);
		}
		else if (input.Key == Key::PageDown)
		{
			handled = true;
			const float lineHeight = GetRenderFont()
				? GetRenderFont()->FontHeight : 16.0f;
			this->_selectionEnd = GetVerticalCaretIndex(
				TextViewportHeight() / (std::max)(1.0f, lineHeight));
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll(true);
		}
		else if (input.Key == Key::Up || input.Key == Key::Down)
		{
			handled = true;
			this->_selectionEnd = GetVerticalCaretIndex(
				input.Key == Key::Up ? -1.0f : 1.0f);
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll();
		}
		else if (input.Key == Key::Return)
		{
			handled = true;
			if (!_isReadOnly && _acceptsReturn)
			{
				InputText(L"\r\n");
				UpdateScroll(true);
			}
		}
		else if (input.Key == Key::Escape)
		{
			handled = true;
		}
		auto eventArgs = input.CreateKeyEventArgs();
		const ControlWeakReference lifetime(this);
		this->OnKeyDown(this, eventArgs);
		handled = handled || eventArgs.Handled;
		// A routed command may commit/cancel an in-place editor and remove this
		// TextBox from its cell while KeyDown is still unwinding.
		if (lifetime.Get() != this) return handled;
		this->InvalidateVisual();
		return handled;
	}
	case InputReportKind::KeyUp:
	{
		auto eventArgs = input.CreateKeyEventArgs();
		const ControlWeakReference lifetime(this);
		this->OnKeyUp(this, eventArgs);
		if (lifetime.Get() != this) return true;
		this->InvalidateVisual();
	}
	break;
	}
	return true;
}
