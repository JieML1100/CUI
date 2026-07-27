#pragma once
#define NOMINMAX
#include "TextBox.h"
#include "Window.h"
#include "TextEditCore.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr float FallbackCornerRadius = 6.0f;
	constexpr float FallbackFocusBorderThickness = 1.6f;
	constexpr auto FallbackHoverColor = cui::theme::palette::SurfaceSubtle;
	constexpr auto FallbackFocusBorderColor = cui::theme::palette::Accent;
	constexpr auto FallbackDisabledOverlayColor =
		cui::theme::palette::DisabledOverlay;

	CuiTextEdit::EditOptions SingleLineEditOptions()
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = false;
		return options;
	}

	void CommitTextChange(TextBox* control, const std::wstring& oldText, const std::wstring& newText)
	{
		if (!control || oldText == newText)
			return;
		// User edits are SetCurrentValue in WPF: update the effective target
		// without replacing a Local Binding or DynamicResource expression.
		(void)control->TrySetCurrentPropertyValue(
			L"Text", BindingValue(newText));
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
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender
			| DependencyPropertyFlags::BindsTwoWayByDefault;
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		options.DefaultUpdateMode = DataSourceUpdateMode::OnValidation;
		options.Changed = [](TextBox& target,
			const std::wstring& oldValue, const std::wstring& newValue)
		{
			TextChangedEventArgs args(oldValue, newValue);
			target.OnTextChanged(&target, args);
		};
		return options;
	}

	DependencyPropertyOptions<TextBox, cui::drawing::Brush> TextBoxBrushOptions(
		cui::drawing::Brush defaultValue,
		int order)
	{
		DependencyPropertyOptions<TextBox, cui::drawing::Brush> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsRender;
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
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Browsable = false;
		return options;
	}

	DependencyPropertyOptions<TextBox, double> SelectionOpacityOptions()
	{
		DependencyPropertyOptions<TextBox, double> options;
		options.DefaultValue = 0.4;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		options.Coerce = [](TextBox&, const double& proposed)
			-> std::optional<double>
		{
			return (std::clamp)(proposed, 0.0, 1.0);
		};
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = 30;
		options.Design.Editor = DependencyPropertyEditorKind::Number;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Minimum = 0.0;
		options.Design.Maximum = 1.0;
		options.Design.Step = 0.05;
		return options;
	}

}

const DependencyProperty& TextBox::TextProperty()
{
	RegisterDependencyProperties();
	const std::type_index ownerTypes[] = {
		std::type_index(typeid(TextBox))
	};
	const auto* metadata =
		DependencyPropertyRegistry::FindRegistered(ownerTypes, L"Text");
	if (!metadata)
		throw std::logic_error(
			"TextBox.Text dependency property is not registered");
	return metadata->Property();
}

void TextBox::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyRegistry::Register<TextBox, std::wstring>(L"Text",
			[](TextBox& target, Handler handler, DataSourceUpdateMode mode)
			{
				if (mode == DataSourceUpdateMode::OnValidation)
					return target.OnLostFocus.Subscribe(
						[handler = std::move(handler)](Control*) { handler(); });
				return target.OnTextChanged.Subscribe(
					[handler = std::move(handler)](
						Control*, TextChangedEventArgs&) { handler(); });
			},
			TextBoxTextOptions());
		DependencyPropertyRegistry::Register<TextBox, cui::drawing::Brush>(
			L"SelectionBrush",
			[](TextBox& target) { return target.SelectionBrush; },
			[](TextBox& target, const cui::drawing::Brush& value)
			{ target.SelectionBrush = value; }, {},
			TextBoxBrushOptions(cui::drawing::MakeSolidColorBrush(
				cui::theme::palette::Accent), 20));
		DependencyPropertyRegistry::Register<TextBox, double>(
			L"SelectionOpacity",
			[](TextBox& target) { return target.SelectionOpacity; },
			[](TextBox& target, const double& value)
			{ target.SelectionOpacity = value; }, {}, SelectionOpacityOptions());
		DependencyPropertyRegistry::Register<TextBox, cui::drawing::Brush>(
			L"SelectionTextBrush",
			[](TextBox& target) { return target.SelectionTextBrush; },
			[](TextBox& target, const cui::drawing::Brush& value)
			{ target.SelectionTextBrush = value; }, {},
			TextBoxBrushOptions(cui::drawing::MakeSolidColorBrush(
				cui::theme::palette::OnAccent), 40));
		DependencyPropertyRegistry::Register<TextBox, cui::drawing::Brush>(
			L"CaretBrush",
			[](TextBox& target) { return target.CaretBrush; },
			[](TextBox& target, const cui::drawing::Brush& value)
			{ target.CaretBrush = value; }, {},
			TextBoxBrushOptions(cui::drawing::MakeSolidColorBrush(
				cui::theme::palette::TextPrimary), 50));
		RegisterControlBorderThicknessMetadata<TextBox>(1.0f, 60);
		return true;
	}();
	(void)registered;
}

GET_CPP(TextBox, std::wstring, Text) { return Control::GetText(); }
SET_CPP(TextBox, std::wstring, Text)
{
	Control::SetText(std::move(value));
}

GET_CPP(TextBox, cui::drawing::Brush, SelectionBrush)
{
	return _selectionBrush;
}
SET_CPP(TextBox, cui::drawing::Brush, SelectionBrush)
{
	(void)SetPropertyField(L"SelectionBrush", _selectionBrush, std::move(value));
}
GET_CPP(TextBox, double, SelectionOpacity)
{
	return _selectionOpacity;
}
SET_CPP(TextBox, double, SelectionOpacity)
{
	(void)SetPropertyField(L"SelectionOpacity", _selectionOpacity, value);
}
GET_CPP(TextBox, cui::drawing::Brush, SelectionTextBrush)
{
	return _selectionTextBrush;
}
SET_CPP(TextBox, cui::drawing::Brush, SelectionTextBrush)
{
	(void)SetPropertyField(
		L"SelectionTextBrush", _selectionTextBrush, std::move(value));
}
GET_CPP(TextBox, cui::drawing::Brush, CaretBrush)
{
	return _caretBrush;
}
SET_CPP(TextBox, cui::drawing::Brush, CaretBrush)
{
	(void)SetPropertyField(L"CaretBrush", _caretBrush, std::move(value));
}
bool TextBox::HandlesNavigationKey(Key key) const
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
TextBox::TextBox()
{
	RegisterDependencyProperties();
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness{ 5.0f }),
		DependencyPropertyValueSource::Theme);
	this->RendererBackgroundColor = cui::theme::palette::Surface;
	this->RendererBorderColor = cui::theme::palette::BorderStrong;
	this->RendererForegroundColor = cui::theme::palette::TextPrimary;
}
void TextBox::InputText(std::wstring input)
{
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto result = CuiTextEdit::ReplaceSelection(newText, this->_selectionStart, this->_selectionEnd, input, SingleLineEditOptions());
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
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	CommitTextChange(this, oldStr, newText);
}
void TextBox::InputBack()
{
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto result = CuiTextEdit::Backspace(newText, this->_selectionStart, this->_selectionEnd, SingleLineEditOptions());
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
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	CommitTextChange(this, oldStr, newText);
}
void TextBox::InputDelete()
{
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto result = CuiTextEdit::DeleteForward(newText, this->_selectionStart, this->_selectionEnd, SingleLineEditOptions());
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
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	CommitTextChange(this, oldStr, newText);
}
void TextBox::ApplyUndoRecord(const UndoRecord& rec, bool isUndo)
{
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
	for (auto& ch : newText)
	{
		if (ch == L'\r' || ch == L'\n') ch = L' ';
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
void TextBox::Undo()
{
	if (this->undoStack.empty()) return;
	UndoRecord rec = this->undoStack.back();
	this->undoStack.pop_back();
	ApplyUndoRecord(rec, true);
	this->redoStack.push_back(rec);
}
void TextBox::Redo()
{
	if (this->redoStack.empty()) return;
	UndoRecord rec = this->redoStack.back();
	this->redoStack.pop_back();
	ApplyUndoRecord(rec, false);
	this->undoStack.push_back(rec);
}
void TextBox::UpdateScroll(bool arrival)
{
	float renderWidth = this->ActualWidth - Padding.Left - Padding.Right;
	auto font = this->GetRenderFont();
	if (!font) return;
	auto hit = font->HitTestTextRange(
		this->Text, static_cast<UINT32>((std::clamp)(
			_selectionEnd, 0, static_cast<int>(this->Text.size()))), 0);
	if (hit.empty()) return;
	const auto& lastSelect = hit.front();
	if ((lastSelect.left + lastSelect.width) - _horizontalScrollOffset > renderWidth)
	{
		_horizontalScrollOffset = (lastSelect.left + lastSelect.width) - renderWidth;
	}
	if (lastSelect.left - _horizontalScrollOffset < 0.0f)
	{
		_horizontalScrollOffset = lastSelect.left;
	}
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
	const int textLen = static_cast<int>(this->Text.size());
	start = (std::clamp)(start, 0, textLen);
	length = (std::clamp)(length, 0, textLen - start);
	this->_selectionStart = start;
	this->_selectionEnd = start + length;
	this->InvalidateVisual();
}

void TextBox::SelectAll()
{
	this->_selectionStart = 0;
	this->_selectionEnd = static_cast<int>(this->Text.size());
	this->InvalidateVisual();
}

void TextBox::ClearSelection()
{
	this->_selectionEnd = this->_selectionStart;
	this->InvalidateVisual();
}

void TextBox::Clear()
{
	this->SelectAll();
	this->InputBack();
}

void TextBox::InsertText(const std::wstring& text)
{
	if (text.empty()) return;
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
	const std::wstring selected = this->GetSelectedString();
	if (selected.empty()) return false;
	if (!WriteClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, selected))
		return false;
	this->InputBack();
	return true;
}

bool TextBox::Paste()
{
	std::wstring clipboardText;
	if (!TryReadClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, clipboardText))
		return false;
	if (clipboardText.empty()) return false;
	this->InputText(clipboardText);
	return true;
}

void TextBox::OnRender()
{
	if (this->IsVisible == false)return;
	bool isUnderMouse = this->IsMouseOver;
	auto d2d = this->GetDrawingContext();
	auto font = this->GetRenderFont();
	float renderHeight = this->ActualHeight - Padding.Top - Padding.Bottom;
	_textSize = font->GetTextSize(this->Text, FLT_MAX, renderHeight);
	float textOffsetY = Padding.Top
		+ (std::max)(0.0f, (renderHeight - _textSize.height) * 0.5f);
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
	bool isSelected = this->GetPresentationWindow()->GetKeyboardFocusedElement() == this;
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
		auto backColor = this->RendererBackgroundColor;
		const float radius = (std::min)(FallbackCornerRadius,
			actualHeight * 0.5f);
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, backColor, radius);
		if ((isUnderMouse || isSelected) && FallbackHoverColor.a > 0.0f)
			d2d->FillRoundRect(1.0f, 1.0f,
				(std::max)(0.0f, actualWidth - 2.0f),
				(std::max)(0.0f, actualHeight - 2.0f),
				FallbackHoverColor, (std::max)(0.0f, radius - 1.0f));
		if (this->Text.size() > 0)
		{
			auto font = this->GetRenderFont();
			if (isSelected)
			{
				int sels = _selectionStart <= _selectionEnd ? _selectionStart : _selectionEnd;
				int sele = _selectionEnd >= _selectionStart ? _selectionEnd : _selectionStart;
				int selLen = sele - sels;
				auto selRange = font->HitTestTextRange(this->Text, (UINT32)sels, (UINT32)selLen);
				if (selLen != 0 && selectionBrush)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(
							sr.left + Padding.Left - _horizontalScrollOffset,
							sr.top + textOffsetY,
							sr.width, sr.height,
							selectionBrush.Get());
					}
				}
				else if (selLen == 0)
				{
					if (!selRange.empty())
					{
						const auto caret = selRange[0];
						const float cx = caret.left + Padding.Left - _horizontalScrollOffset;
						const float cy = caret.top + textOffsetY;
						const float ch = caret.height > 0 ? caret.height : font->FontHeight;
						const auto absoluteLocation = this->GetAbsoluteLocationDip();
						this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ch + 2.0f };
						this->_caretRectCacheValid = true;
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + Padding.Left - _horizontalScrollOffset, selRange[0].top + textOffsetY };
						caretEnd = { selRange[0].left + Padding.Left - _horizontalScrollOffset, selRange[0].top + selRange[0].height + textOffsetY };
					}
				}
				auto textLayout = Factory::CreateStringLayout(this->Text, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
					if (selectionTextBrush && foreground)
						d2d->DrawStringLayoutEffect(textLayout,
							Padding.Left - _horizontalScrollOffset, textOffsetY,
							foreground.Get(),
							DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
							selectionTextBrush.Get(),
							font);
					else if (selectionTextBrush)
						d2d->DrawStringLayoutEffect(textLayout,
							Padding.Left - _horizontalScrollOffset, textOffsetY,
							this->RendererForegroundColor,
							DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
							selectionTextBrush.Get(),
							font);
					else if (foreground)
						d2d->DrawStringLayout(textLayout,
							Padding.Left - _horizontalScrollOffset, textOffsetY,
							foreground.Get());
					else
						d2d->DrawStringLayout(textLayout,
							Padding.Left - _horizontalScrollOffset, textOffsetY,
							this->RendererForegroundColor);
					textLayout->Release();
				}
			}
			else
			{
				auto textLayout = Factory::CreateStringLayout(this->Text, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
					if (foreground)
						d2d->DrawStringLayout(textLayout,
							Padding.Left - _horizontalScrollOffset, textOffsetY,
							foreground.Get());
					else
						d2d->DrawStringLayout(textLayout,
							Padding.Left - _horizontalScrollOffset, textOffsetY,
							this->RendererForegroundColor);
					textLayout->Release();
				}
			}
		}
		else
		{
			if (isSelected)
			{
				const float cx = Padding.Left - _horizontalScrollOffset;
				const float cy = textOffsetY;
				const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				const auto absoluteLocation = this->GetAbsoluteLocationDip();
				this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ch + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { Padding.Left - _horizontalScrollOffset, textOffsetY };
				caretEnd = { Padding.Left - _horizontalScrollOffset, textOffsetY + 16.0f };
			}
		}
		UpdateCaretBlinkState(isSelected, this->_selectionStart, this->_selectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible() && caretBrush)
		{
			d2d->DrawLine(caretStart, caretEnd, caretBrush.Get());
		}
		const auto borderColor = isSelected
			? FallbackFocusBorderColor : this->RendererBorderColor;
		const float borderWidth = isSelected
			? (std::max)(this->BorderThickness.MaxEdge(),
				FallbackFocusBorderThickness)
			: this->BorderThickness.MaxEdge();
		if (borderWidth > 0.0f && borderColor.a > 0.0f)
			d2d->DrawRoundRect(borderWidth * 0.5f, borderWidth * 0.5f,
				(std::max)(0.0f, actualWidth - borderWidth), (std::max)(0.0f, actualHeight - borderWidth),
				borderColor, borderWidth, radius);
		if (!this->IsEnabled)
		{
			d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight,
				FallbackDisabledOverlayColor, radius);
		}
	}
	this->EndRender();
}

bool TextBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}

bool TextBox::ApplyTextInput(const TextCompositionEventArgs& input)
{
	if (input.Text.empty()) return false;
	InputText(input.Text);
	UpdateScroll();
	InvalidateVisual();
	return true;
}

bool TextBox::TryGetTextInputCaretRect(D2D1_RECT_F& outRect)
{
	if (_caretRectCacheValid)
	{
		outRect = _caretRectCache;
		return true;
	}
	const auto absolute = GetAbsoluteLocationDip();
	const float x = static_cast<float>(absolute.x)
		+ Padding.Left - _horizontalScrollOffset;
	const float y = static_cast<float>(absolute.y) + Padding.Top;
	const float height = GetRenderFont() && GetRenderFont()->FontHeight > 0.0f
		? GetRenderFont()->FontHeight : 16.0f;
	outRect = D2D1::RectF(x, y, x + 1.0f, y + height);
	return true;
}

bool TextBox::ProcessInput(const InputReport& input)
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
			_selectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX,
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
			this->_selectionStart = this->_selectionEnd =
				font->HitTestTextPosition(this->Text, FLT_MAX, renderHeight,
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
			_selectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX,
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
		this->_selectionEnd = static_cast<int>(this->Text.size());
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
			if (input.Key == Key::C)
			{
				(void)Copy();
				return true;
			}
			if (input.Key == Key::V)
			{
				(void)Paste();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::X)
			{
				(void)Cut();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::Z)
			{
				this->Undo();
				UpdateScroll();
				this->InvalidateVisual();
				return true;
			}
			if (input.Key == Key::Y)
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
			this->_selectionEnd = 0;
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll();
		}
		else if (input.Key == Key::End)
		{
			handled = true;
			this->_selectionEnd = static_cast<int>(this->Text.size());
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
			this->_selectionEnd = static_cast<int>(this->Text.size());
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll(true);
		}
		else if (input.Key == Key::Up || input.Key == Key::Down
			|| input.Key == Key::Return || input.Key == Key::Escape)
		{
			// Single-line editors own directional navigation and reject
			// line-break/control characters without consuming text-producing
			// keys such as Space.
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
