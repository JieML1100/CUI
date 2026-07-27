#pragma once
#define NOMINMAX
#include "PasswordBox.h"
#include "Window.h"
#include "TextEditCore.h"
#include <cstring>

namespace
{
	constexpr float FallbackCornerRadius = 6.0f;
	constexpr float FallbackFocusBorderThickness = 1.6f;
	constexpr auto FallbackHoverColor = cui::theme::palette::SurfaceSubtle;
	constexpr auto FallbackFocusBorderColor = cui::theme::palette::Accent;
	constexpr auto FallbackDisabledOverlayColor =
		cui::theme::palette::DisabledOverlay;

	CuiTextEdit::EditOptions PasswordEditOptions()
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = false;
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

void PasswordBox::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<PasswordBox, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		options.Changed = [](
			PasswordBox& target, const std::wstring&, const std::wstring&)
		{
			target.PublishPasswordChanged();
		};
		DependencyPropertyRegistry::Register<PasswordBox, std::wstring>(
			L"Password",
			[](PasswordBox& target) { return target.Password; },
			[](PasswordBox& target, const std::wstring& value)
			{ target.Password = value; },
			[](PasswordBox& target, Handler handler, DataSourceUpdateMode mode)
			{
				if (mode == DataSourceUpdateMode::OnValidation)
					return target.OnLostFocus.Subscribe(
						[handler = std::move(handler)](Control*) { handler(); });
				return target.PasswordChanged.Subscribe(
					[handler = std::move(handler)](
						Control*, RoutedEventArgs&) { handler(); });
			}, std::move(options));
		RegisterControlBorderThicknessMetadata<PasswordBox>(1.0f, 60);
		return true;
	}();
	(void)registered;
}

GET_CPP(PasswordBox, std::wstring, Password)
{
	return _password;
}

SET_CPP(PasswordBox, std::wstring, Password)
{
	(void)SetPropertyField(L"Password", _password, std::move(value));
}

void PasswordBox::CommitPasswordEdit(std::wstring value)
{
	if (_password == value) return;
	_password = std::move(value);
	PublishPasswordChanged();
}

void PasswordBox::PublishPasswordChanged()
{
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
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness{ 5.0f }),
		DependencyPropertyValueSource::Theme);
	this->RendererBackgroundColor = cui::theme::palette::Surface;
	this->RendererBorderColor = cui::theme::palette::BorderStrong;
	this->RendererForegroundColor = cui::theme::palette::TextPrimary;
}
void PasswordBox::InputText(std::wstring input)
{
	std::wstring newText = this->Password;
	CuiTextEdit::ReplaceSelection(newText, this->_selectionStart, this->_selectionEnd, input, PasswordEditOptions());
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
	float renderWidth = (std::max)(
		0.0f, this->ActualWidth - Padding.Left - Padding.Right);
	auto font = this->GetRenderFont();
	if (!font)
	{
		_horizontalScrollOffset = 0.0f;
		return;
	}
	std::wstring maskedText = this->GetDisplayText();
	auto hit = font->HitTestTextRange(
		maskedText,
		static_cast<UINT32>((std::clamp)(
			_selectionEnd, 0, static_cast<int>(maskedText.size()))),
		0);
	if (hit.empty())
	{
		_horizontalScrollOffset = 0.0f;
		return;
	}
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
std::wstring PasswordBox::GetSelectedString()
{
	auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Password.size());
	if (!span.HasSelection())
		return L"";
	return this->Password.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}

std::wstring PasswordBox::GetDisplayText()
{
	return std::wstring(this->Password.size(), _passwordChar);
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
	if (text.empty()) return;
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

void PasswordBox::OnRender()
{
	if (this->IsVisible == false)return;
	bool isUnderMouse = this->IsMouseOver;
	auto d2d = this->GetDrawingContext();
	auto font = this->GetRenderFont();
	float renderHeight = this->ActualHeight - Padding.Top - Padding.Bottom;
	std::wstring maskedText = this->GetDisplayText();
	_textSize = font->GetTextSize(maskedText, FLT_MAX, renderHeight);
	float textOffsetY = Padding.Top
		+ (std::max)(0.0f, (renderHeight - _textSize.height) * 0.5f);
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
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
		if (this->Password.size() > 0)
		{
			auto font = this->GetRenderFont();
			if (isSelected)
			{
				int sels = _selectionStart <= _selectionEnd ? _selectionStart : _selectionEnd;
				int sele = _selectionEnd >= _selectionStart ? _selectionEnd : _selectionStart;
				int selLen = sele - sels;
				auto selRange = font->HitTestTextRange(maskedText, (UINT32)sels, (UINT32)selLen);
				if (selLen != 0)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(sr.left + Padding.Left - _horizontalScrollOffset,
							sr.top + textOffsetY, sr.width, sr.height,
							_selectedBackColor);
					}
				}
				else
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
				auto textLayout = Factory::CreateStringLayout(maskedText, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
					d2d->DrawStringLayoutEffect(textLayout,
						Padding.Left - _horizontalScrollOffset, textOffsetY,
						this->RendererForegroundColor,
						DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
						_selectedForeColor,
						font);
					textLayout->Release();
				}
			}
			else
			{
				auto textLayout = Factory::CreateStringLayout(maskedText, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
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
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->RendererForegroundColor);
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
	}
	if (!this->IsEnabled)
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight,
			FallbackDisabledOverlayColor,
			(std::min)(FallbackCornerRadius, actualHeight * 0.5f));
	}
	this->EndRender();
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
