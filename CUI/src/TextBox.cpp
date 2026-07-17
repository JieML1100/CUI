#pragma once
#define NOMINMAX
#include "TextBox.h"
#include "Form.h"
#include "TextEditCore.h"
#include <cstring>
#pragma comment(lib, "Imm32.lib")

namespace
{
	CuiTextEdit::EditOptions SingleLineEditOptions()
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
	bool TextBoxColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<TextBox, D2D1_COLOR_F> TextBoxColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = ControlPropertyOptions<TextBox, D2D1_COLOR_F>{
			defaultValue,
			ControlPropertyFlags::AffectsRender
				| ControlPropertyFlags::TracksLocalValue,
			{}, {}, TextBoxColorsEqual };
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = order;
		options.Design.Editor = ControlPropertyEditorKind::Color;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	ControlPropertyOptions<TextBox, float> TextBoxNonNegativeFloatOptions(
		float defaultValue,
		int order)
	{
		auto options = ControlPropertyOptions<TextBox, float>{
			defaultValue,
			ControlPropertyFlags::AffectsRender
				| ControlPropertyFlags::TracksLocalValue,
			[](TextBox&, const float& proposed) -> std::optional<float>
			{
				return (std::max)(0.0f, proposed);
			} };
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = order;
		options.Design.Editor = ControlPropertyEditorKind::Number;
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}
}

void TextBox::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		BindingPropertyRegistry::Register<TextBox, D2D1_COLOR_F>(L"UnderMouseColor",
			[](TextBox& target) { return target.UnderMouseColor; },
			[](TextBox& target, const D2D1_COLOR_F& value) { target.UnderMouseColor = value; },
			{}, TextBoxColorOptions(cui::theme::palette::SurfaceSubtle, 10));
		BindingPropertyRegistry::Register<TextBox, D2D1_COLOR_F>(L"SelectedBackColor",
			[](TextBox& target) { return target.SelectedBackColor; },
			[](TextBox& target, const D2D1_COLOR_F& value) { target.SelectedBackColor = value; },
			{}, TextBoxColorOptions(cui::theme::palette::SelectionBack, 20));
		BindingPropertyRegistry::Register<TextBox, D2D1_COLOR_F>(L"SelectedForeColor",
			[](TextBox& target) { return target.SelectedForeColor; },
			[](TextBox& target, const D2D1_COLOR_F& value) { target.SelectedForeColor = value; },
			{}, TextBoxColorOptions(cui::theme::palette::TextPrimary, 30));
		BindingPropertyRegistry::Register<TextBox, D2D1_COLOR_F>(L"FocusedColor",
			[](TextBox& target) { return target.FocusedColor; },
			[](TextBox& target, const D2D1_COLOR_F& value) { target.FocusedColor = value; },
			{}, TextBoxColorOptions(cui::theme::palette::Surface, 40));
		BindingPropertyRegistry::Register<TextBox, D2D1_COLOR_F>(L"DisabledOverlayColor",
			[](TextBox& target) { return target.DisabledOverlayColor; },
			[](TextBox& target, const D2D1_COLOR_F& value) { target.DisabledOverlayColor = value; },
			{}, TextBoxColorOptions(cui::theme::palette::DisabledOverlay, 50));
		BindingPropertyRegistry::Register<TextBox, float>(L"BorderThickness",
			[](TextBox& target) { return target.BorderThickness; },
			[](TextBox& target, const float& value) { target.BorderThickness = value; },
			{}, TextBoxNonNegativeFloatOptions(1.0f, 60));
		BindingPropertyRegistry::Register<TextBox, float>(L"CornerRadius",
			[](TextBox& target) { return target.CornerRadius; },
			[](TextBox& target, const float& value) { target.CornerRadius = value; },
			{}, TextBoxNonNegativeFloatOptions(6.0f, 70));
		BindingPropertyRegistry::Register<TextBox, float>(L"FocusBorder",
			[](TextBox& target) { return target.FocusBorder; },
			[](TextBox& target, const float& value) { target.FocusBorder = value; },
			{}, TextBoxNonNegativeFloatOptions(1.6f, 80));
		BindingPropertyRegistry::Register<TextBox, float>(L"TextMargin",
			[](TextBox& target) { return target.TextMargin; },
			[](TextBox& target, const float& value) { target.TextMargin = value; },
			{}, TextBoxNonNegativeFloatOptions(5.0f, 90));
		return true;
	}();
	(void)registered;
}

GET_CPP(TextBox, D2D1_COLOR_F, UnderMouseColor) { return _underMouseColor; }
SET_CPP(TextBox, D2D1_COLOR_F, UnderMouseColor) { SetPropertyField(L"UnderMouseColor", _underMouseColor, value); }
GET_CPP(TextBox, D2D1_COLOR_F, SelectedBackColor) { return _selectedBackColor; }
SET_CPP(TextBox, D2D1_COLOR_F, SelectedBackColor) { SetPropertyField(L"SelectedBackColor", _selectedBackColor, value); }
GET_CPP(TextBox, D2D1_COLOR_F, SelectedForeColor) { return _selectedForeColor; }
SET_CPP(TextBox, D2D1_COLOR_F, SelectedForeColor) { SetPropertyField(L"SelectedForeColor", _selectedForeColor, value); }
GET_CPP(TextBox, D2D1_COLOR_F, FocusedColor) { return _focusedColor; }
SET_CPP(TextBox, D2D1_COLOR_F, FocusedColor) { SetPropertyField(L"FocusedColor", _focusedColor, value); }
GET_CPP(TextBox, D2D1_COLOR_F, DisabledOverlayColor) { return _disabledOverlayColor; }
SET_CPP(TextBox, D2D1_COLOR_F, DisabledOverlayColor) { SetPropertyField(L"DisabledOverlayColor", _disabledOverlayColor, value); }
GET_CPP(TextBox, float, BorderThickness) { return _borderThickness; }
SET_CPP(TextBox, float, BorderThickness) { SetPropertyField(L"BorderThickness", _borderThickness, value); }
GET_CPP(TextBox, float, CornerRadius) { return _cornerRadius; }
SET_CPP(TextBox, float, CornerRadius) { SetPropertyField(L"CornerRadius", _cornerRadius, value); }
GET_CPP(TextBox, float, FocusBorder) { return _focusBorder; }
SET_CPP(TextBox, float, FocusBorder) { SetPropertyField(L"FocusBorder", _focusBorder, value); }
GET_CPP(TextBox, float, TextMargin) { return _textMargin; }
SET_CPP(TextBox, float, TextMargin) { SetPropertyField(L"TextMargin", _textMargin, value); }

bool TextBox::HandlesNavigationKey(WPARAM key) const
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
TextBox::TextBox(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::BorderStrong;
	this->ForeColor = cui::theme::palette::TextPrimary;
}
void TextBox::InputText(std::wstring input)
{
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->SelectionStart;
	const int selEndBefore = this->SelectionEnd;
	auto result = CuiTextEdit::ReplaceSelection(newText, this->SelectionStart, this->SelectionEnd, input, SingleLineEditOptions());
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = result.insertedText;
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	CommitTextChange(this, oldStr, newText);
}
void TextBox::InputBack()
{
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->SelectionStart;
	const int selEndBefore = this->SelectionEnd;
	auto result = CuiTextEdit::Backspace(newText, this->SelectionStart, this->SelectionEnd, SingleLineEditOptions());
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	CommitTextChange(this, oldStr, newText);
}
void TextBox::InputDelete()
{
	std::wstring oldStr = this->Text;
	std::wstring newText = this->Text;
	const int selStartBefore = this->SelectionStart;
	const int selEndBefore = this->SelectionEnd;
	auto result = CuiTextEdit::DeleteForward(newText, this->SelectionStart, this->SelectionEnd, SingleLineEditOptions());
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
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
		this->SelectionStart = rec.selStartBefore;
		this->SelectionEnd = rec.selEndBefore;
	}
	else
	{
		this->SelectionStart = rec.selStartAfter;
		this->SelectionEnd = rec.selEndAfter;
	}
	this->SelectionStart = std::clamp(this->SelectionStart, 0, (int)newText.size());
	this->SelectionEnd = std::clamp(this->SelectionEnd, 0, (int)newText.size());

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
	float renderWidth = this->Width - (TextMargin * 2.0f);
	auto font = this->Font;
	auto lastSelect = font->HitTestTextRange(this->Text, (UINT32)SelectionEnd, (UINT32)0)[0];
	if ((lastSelect.left + lastSelect.width) - HorizontalScrollOffset > renderWidth)
	{
		HorizontalScrollOffset = (lastSelect.left + lastSelect.width) - renderWidth;
	}
	if (lastSelect.left - HorizontalScrollOffset < 0.0f)
	{
		HorizontalScrollOffset = lastSelect.left;
	}
}
std::wstring TextBox::GetSelectedString()
{
	auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->Text.size());
	if (!span.HasSelection())
		return L"";
	return this->Text.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}

// ---- 公共选择/编辑 API ----
int TextBox::GetSelectionLength()
{
	auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->Text.size());
	return span.HasSelection() ? static_cast<int>(span.Length()) : 0;
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
	this->SelectionStart = start;
	this->SelectionEnd = start + length;
	this->InvalidateVisual();
}

void TextBox::SelectAll()
{
	this->SelectionStart = 0;
	this->SelectionEnd = static_cast<int>(this->Text.size());
	this->InvalidateVisual();
}

void TextBox::ClearSelection()
{
	this->SelectionEnd = this->SelectionStart;
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
	return WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, selected);
}

bool TextBox::Cut()
{
	const std::wstring selected = this->GetSelectedString();
	if (selected.empty()) return false;
	if (!WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, selected))
		return false;
	this->InputBack();
	return true;
}

bool TextBox::Paste()
{
	std::wstring clipboardText;
	if (!TryReadClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, clipboardText))
		return false;
	if (clipboardText.empty()) return false;
	this->InputText(clipboardText);
	return true;
}

void TextBox::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	float renderHeight = this->Height - (TextMargin * 2.0f);
	textSize = font->GetTextSize(this->Text, FLT_MAX, renderHeight);
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
				auto selRange = font->HitTestTextRange(this->Text, (UINT32)sels, (UINT32)selLen);
				if (selLen != 0)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(
							sr.left + TextMargin - HorizontalScrollOffset,
							sr.top + textOffsetY,
							sr.width, sr.height,
							this->SelectedBackColor);
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
				auto textLayout = Factory::CreateStringLayout(this->Text, FLT_MAX, renderHeight, font->FontObject);
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
				auto textLayout = Factory::CreateStringLayout(this->Text, FLT_MAX, renderHeight, font->FontObject);
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
		if (!this->Enable)
		{
			d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, radius);
		}
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, (std::min)(CornerRadius, actualHeight * 0.5f));
	}
	this->EndRender();
}

bool TextBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}
bool TextBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);
		TCHAR fileName[MAX_PATH];
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
			SelectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX, renderHeight, (localX - TextMargin) + this->HorizontalScrollOffset, localY - TextMargin);
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
			this->SelectionStart = this->SelectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX, renderHeight, (localX - TextMargin) + this->HorizontalScrollOffset, localY - TextMargin);
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
			SelectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX, renderHeight, (localX - TextMargin) + this->HorizontalScrollOffset, localY - TextMargin);
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
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
		{
			if (wParam == 'Z')
			{
				this->Undo();
				UpdateScroll();
				this->InvalidateVisual();
				return true;
			}
			if (wParam == 'Y')
			{
				this->Redo();
				UpdateScroll();
				this->InvalidateVisual();
				return true;
			}
		}
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
		else if (ch == 3)
		{
			std::wstring s = this->GetSelectedString();
			WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, s);
		}
		else if (ch == 24)
		{
			std::wstring s = this->GetSelectedString();
			WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, s);
			this->InputBack();
			UpdateScroll();
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
