#pragma once
#define NOMINMAX
#include "PasswordBox.h"
#include "Form.h"
#include "TextEditCore.h"
#include <cstring>
#pragma comment(lib, "Imm32.lib")

namespace
{
	CuiTextEdit::EditOptions PasswordEditOptions()
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
bool PasswordBox::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return false;
	}
}
PasswordBox::PasswordBox(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::BorderStrong;
	this->ForeColor = cui::theme::palette::TextPrimary;
}
void PasswordBox::InputText(std::wstring input)
{
	std::wstring oldText = this->Text;
	std::wstring newText = this->Text;
	CuiTextEdit::ReplaceSelection(newText, this->SelectionStart, this->SelectionEnd, input, PasswordEditOptions());
	CommitTextChange(this, oldText, newText);
}
void PasswordBox::InputBack()
{
	std::wstring oldText = this->Text;
	std::wstring newText = this->Text;
	CuiTextEdit::Backspace(newText, this->SelectionStart, this->SelectionEnd, PasswordEditOptions());
	CommitTextChange(this, oldText, newText);
}
void PasswordBox::InputDelete()
{
	std::wstring oldText = this->Text;
	std::wstring newText = this->Text;
	CuiTextEdit::DeleteForward(newText, this->SelectionStart, this->SelectionEnd, PasswordEditOptions());
	CommitTextChange(this, oldText, newText);
}
void PasswordBox::UpdateScroll(bool arrival)
{
	float renderWidth = this->Width - (TextMargin * 2.0f);
	auto font = this->Font;
	std::wstring maskedText(this->Text.size(), L'*');
	auto lastSelect = font->HitTestTextRange(maskedText, (UINT32)SelectionEnd, (UINT32)0)[0];
	if ((lastSelect.left + lastSelect.width) - HorizontalScrollOffset > renderWidth)
	{
		HorizontalScrollOffset = (lastSelect.left + lastSelect.width) - renderWidth;
	}
	if (lastSelect.left - HorizontalScrollOffset < 0.0f)
	{
		HorizontalScrollOffset = lastSelect.left;
	}
}
std::wstring PasswordBox::GetSelectedString()
{
	auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->Text.size());
	if (!span.HasSelection())
		return L"";
	return this->Text.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}
void PasswordBox::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	float renderHeight = this->Height - (TextMargin * 2.0f);
	std::wstring maskedText(this->Text.size(), L'*');
	textSize = font->GetTextSize(maskedText, FLT_MAX, renderHeight);
	float textOffsetY = (this->Height - textSize.height) * 0.5f;
	if (textOffsetY < 0.0f) textOffsetY = 0.0f;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	bool isSelected = this->ParentForm->Selected == this;
	this->_caretRectCacheValid = false;
	bool shouldDrawCaret = false;
	D2D1_POINT_2F caretStart{};
	D2D1_POINT_2F caretEnd{};
	this->BeginRender();
	{
		auto backColor = this->BackColor;
		const float radius = (std::min)(CornerRadius, actualHeight * 0.5f);
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, backColor, radius);
		if ((isUnderMouse || isSelected) && this->UnderMouseColor.a > 0.0f)
			d2d->FillRoundRect(1.0f, 1.0f, (std::max)(0.0f, actualWidth - 2.0f), (std::max)(0.0f, actualHeight - 2.0f), this->UnderMouseColor, (std::max)(0.0f, radius - 1.0f));
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		if (this->Text.size() > 0)
		{
			auto font = this->Font;
			if (isSelected)
			{
				int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
				int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
				int selLen = sele - sels;
				auto selRange = font->HitTestTextRange(maskedText, (UINT32)sels, (UINT32)selLen);
				if (selLen != 0)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(sr.left + TextMargin - HorizontalScrollOffset, sr.top + textOffsetY, sr.width, sr.height, this->SelectedBackColor);
					}
				}
				else
				{
					if (!selRange.empty())
					{
						const auto caret = selRange[0];
						const float cx = caret.left + TextMargin - HorizontalScrollOffset;
						const float cy = caret.top + textOffsetY;
						const float ch = caret.height > 0 ? caret.height : font->FontHeight;
						const auto absoluteLocation = this->GetAbsoluteLocationDip();
						this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ch + 2.0f };
						this->_caretRectCacheValid = true;
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + TextMargin - HorizontalScrollOffset, selRange[0].top + textOffsetY };
						caretEnd = { selRange[0].left + TextMargin - HorizontalScrollOffset, selRange[0].top + selRange[0].height + textOffsetY };
					}
				}
				auto textLayout = Factory::CreateStringLayout(maskedText, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
					d2d->DrawStringLayoutEffect(textLayout,
						TextMargin - HorizontalScrollOffset, textOffsetY,
						this->ForeColor,
						DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
						this->SelectedForeColor,
						font);
					textLayout->Release();
				}
			}
			else
			{
				auto textLayout = Factory::CreateStringLayout(maskedText, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
					d2d->DrawStringLayout(textLayout,
						TextMargin - HorizontalScrollOffset, textOffsetY,
						this->ForeColor);
					textLayout->Release();
				}
			}
		}
		else
		{
			if (isSelected)
			{
				const float cx = (float)TextMargin - HorizontalScrollOffset;
				const float cy = textOffsetY;
				const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				const auto absoluteLocation = this->GetAbsoluteLocationDip();
				this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ch + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { (float)TextMargin - HorizontalScrollOffset, textOffsetY };
				caretEnd = { (float)TextMargin - HorizontalScrollOffset, textOffsetY + 16.0f };
			}
		}
		UpdateCaretBlinkState(isSelected, this->SelectionStart, this->SelectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->ForeColor);
		}
		const auto borderColor = isSelected ? this->FocusedColor : this->BorderColor;
		const float borderWidth = isSelected ? (std::max)(this->BorderThickness, this->FocusBorder) : this->BorderThickness;
		if (borderWidth > 0.0f && borderColor.a > 0.0f)
			d2d->DrawRoundRect(borderWidth * 0.5f, borderWidth * 0.5f,
				(std::max)(0.0f, actualWidth - borderWidth), (std::max)(0.0f, actualHeight - borderWidth),
				borderColor, borderWidth, radius);
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, (std::min)(CornerRadius, actualHeight * 0.5f));
	}
	this->EndRender();
}

bool PasswordBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}
bool PasswordBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
		TCHAR fileName[MAX_PATH]{};
		std::vector<std::wstring> files;
		for (UINT fileIndex = 0; fileIndex < fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (!files.empty())
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && this->ParentForm->Selected == this)
		{
			auto font = this->Font;
			float renderHeight = this->Height - (TextMargin * 2.0f);
			std::wstring maskedText(this->Text.size(), L'*');
			SelectionEnd = font->HitTestTextPosition(maskedText, FLT_MAX, renderHeight, (localX - TextMargin) + this->HorizontalScrollOffset, localY - TextMargin);
			UpdateScroll();
			this->InvalidateVisual();
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
		{
			if (this->ParentForm->Selected != this)
			{
				auto previousSelection = this->ParentForm->Selected;
				this->ParentForm->SetSelectedControl(this, false);
				if (previousSelection) previousSelection->InvalidateVisual();
			}
			auto font = this->Font;
			float renderHeight = this->Height - (TextMargin * 2.0f);
			std::wstring maskedText(this->Text.size(), L'*');
			this->SelectionStart = this->SelectionEnd = font->HitTestTextPosition(maskedText, FLT_MAX, renderHeight, (localX - TextMargin) + this->HorizontalScrollOffset, localY - TextMargin);
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (this->ParentForm->Selected == this)
		{
			float renderHeight = this->Height - (TextMargin * 2.0f);
			auto font = this->Font;
			std::wstring maskedText(this->Text.size(), L'*');
			SelectionEnd = font->HitTestTextPosition(maskedText, FLT_MAX, renderHeight, (localX - TextMargin) + this->HorizontalScrollOffset, localY - TextMargin);
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		this->ParentForm->SetSelectedControl(this, false);
		this->SelectionStart = 0;
		this->SelectionEnd = static_cast<int>(this->Text.size());
		this->HorizontalScrollOffset = 0.0f;
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_KEYDOWN:
	{
		if (this->ParentForm)
		{
			D2D1_RECT_F imeRect{};
			if (this->_caretRectCacheValid)
			{
				imeRect = this->_caretRectCache;
			}
			else
			{
				const auto absoluteLocation = this->GetAbsoluteLocationDip();
				float caretX = (float)absoluteLocation.x + this->TextMargin - this->HorizontalScrollOffset;
				float caretY = (float)absoluteLocation.y;
				float caretH = (this->Font && this->Font->FontHeight > 0.0f) ? this->Font->FontHeight : 16.0f;
				imeRect = D2D1_RECT_F{ caretX, caretY, caretX + 1.0f, caretY + caretH };
			}
			this->ParentForm->SetImeCompositionWindowFromLogicalRect(imeRect);
		}
		if (wParam == VK_DELETE)
		{
			this->InputDelete();
			UpdateScroll();
		}
		else if (wParam == VK_RIGHT)
		{
			int textLength = static_cast<int>(this->Text.size());
			const bool extendSelection = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->Text.size());
			if (!extendSelection && span.HasSelection())
			{
				this->SelectionStart = this->SelectionEnd = span.end;
				UpdateScroll();
			}
			else if (this->SelectionEnd < textLength)
			{
				this->SelectionEnd = CuiTextEdit::GetNextCaretIndex(this->Text, this->SelectionEnd, false);
				if (!extendSelection)
					this->SelectionStart = this->SelectionEnd;
				UpdateScroll();
			}
		}
		else if (wParam == VK_LEFT)
		{
			const bool extendSelection = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->Text.size());
			if (!extendSelection && span.HasSelection())
			{
				this->SelectionStart = this->SelectionEnd = span.start;
				UpdateScroll();
			}
			else if (this->SelectionEnd > 0)
			{
				this->SelectionEnd = CuiTextEdit::GetPreviousCaretIndex(this->Text, this->SelectionEnd, false);
				if (!extendSelection)
					this->SelectionStart = this->SelectionEnd;
				UpdateScroll();
			}
		}
		else if (wParam == VK_HOME)
		{
			this->SelectionEnd = 0;
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->SelectionStart = this->SelectionEnd;
			UpdateScroll();
		}
		else if (wParam == VK_END)
		{
			this->SelectionEnd = static_cast<int>(this->Text.size());
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->SelectionStart = this->SelectionEnd;
			UpdateScroll();
		}
		else if (wParam == VK_PRIOR)
		{
			this->SelectionEnd = 0;
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->SelectionStart = this->SelectionEnd;
			UpdateScroll(true);
		}
		else if (wParam == VK_NEXT)
		{
			this->SelectionEnd = static_cast<int>(this->Text.size());
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->SelectionStart = this->SelectionEnd;
			UpdateScroll(true);
		}
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_CHAR:
	{
		wchar_t ch = (wchar_t)(wParam);
		if (CuiTextEdit::IsTextInputChar(ch))
		{
			const wchar_t c[] = { ch,L'\0' };
			this->InputText(c);
			UpdateScroll();
		}
		else if (ch == 1)
		{
			this->SelectionStart = 0;
			this->SelectionEnd = static_cast<int>(this->Text.size());
			UpdateScroll();
		}
		else if (ch == 8)
		{
			if (this->Text.size() > 0)
			{
				this->InputBack();
				UpdateScroll();
			}
		}
		else if (ch == 22)
		{
			std::wstring clipboardText;
			if (TryReadClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, clipboardText))
			{
				this->InputText(clipboardText);
				UpdateScroll();
			}
		}
		this->InvalidateVisual();
	}
	break;
	case WM_IME_COMPOSITION:
	{
		if (lParam & GCS_RESULTSTR)
		{
			// Unicode windows receive committed IME text through WM_CHAR as well.
			// Keep the edit mutation in one path to avoid duplicate characters.
			this->InvalidateVisual();
		}
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	}
	return true;
}
