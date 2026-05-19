#pragma once
#define NOMINMAX
#include "TextBox.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")

namespace
{
	std::wstring BuildTextFromBuffer(std::vector<wchar_t>& buffer)
	{
		if (buffer.empty() || buffer.back() != L'\0')
		{
			buffer.push_back(L'\0');
		}
		return std::wstring(buffer.data());
	}
}

UIClass TextBox::Type() { return UIClass::UI_TextBox; }
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
	this->BackColor = Colors::LightGray;
}
void TextBox::InputText(std::wstring input)
{
	std::wstring oldStr = this->Text;
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int textLength = static_cast<int>(this->Text.size());
	int inputLength = static_cast<int>(input.size());
	UndoRecord rec;
	bool shouldRecord = false;
	int recSels = std::clamp(sels, 0, textLength);
	int recSele = std::clamp(sele, 0, textLength);
	if (!this->isApplyingUndoRedo && (!input.empty() || recSele > recSels))
	{
		rec.pos = recSels;
		rec.removedText = (recSele > recSels) ? this->Text.substr((size_t)recSels, (size_t)(recSele - recSels)) : L"";
		rec.insertedText = input;
		for (auto& ch : rec.insertedText)
		{
			if (ch == L'\r' || ch == L'\n') ch = L' ';
		}
		rec.selStartBefore = this->SelectionStart;
		rec.selEndBefore = this->SelectionEnd;
		shouldRecord = true;
	}
	if (sele >= textLength && sels >= textLength)
	{
		this->Text += input;
		SelectionEnd = SelectionStart = static_cast<int>(this->Text.size());
	}
	else
	{
		std::vector<wchar_t> editBuffer = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
		if (sele > sels)
		{
			int removeLength = sele - sels;
			for (int i = 0; i < removeLength; i++)
			{
				editBuffer.erase(editBuffer.begin() + sels);
			}
			for (int i = 0; i < inputLength; i++)
			{
				editBuffer.insert(editBuffer.begin() + sels + i, input[i]);
			}
			SelectionEnd = SelectionStart = sels + inputLength;
			this->Text = BuildTextFromBuffer(editBuffer);
		}
		else if (sele == sels && sele >= 0)
		{
			for (int i = 0; i < inputLength; i++)
			{
				editBuffer.insert(editBuffer.begin() + sels + i, input[i]);
			}
			SelectionEnd += inputLength;
			SelectionStart += inputLength;
			this->Text = BuildTextFromBuffer(editBuffer);
		}
		else
		{
			this->Text += input;
			SelectionEnd = SelectionStart = static_cast<int>(this->Text.size());
		}
	}
	if (this->Text.empty())
	{
		this->Text = L"";
	}
	std::vector<wchar_t> editBuffer = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
	for (size_t i = 0; i < editBuffer.size(); i++)
	{
		if (editBuffer[i] == L'\r' || editBuffer[i] == L'\n')
		{
			editBuffer[i] = L' ';
		}
	}
	this->Text = BuildTextFromBuffer(editBuffer);
	if (shouldRecord)
	{
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	this->OnTextChanged(this, oldStr, this->Text);
}
void TextBox::InputBack()
{
	std::wstring oldStr = this->Text;
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int selLen = sele - sels;
	UndoRecord rec;
	bool shouldRecord = false;
	if (selLen > 0)
	{
		if (!this->isApplyingUndoRedo)
		{
			int recSels = std::clamp(sels, 0, (int)this->Text.size());
			int recSele = std::clamp(sele, 0, (int)this->Text.size());
			rec.pos = recSels;
			rec.removedText = (recSele > recSels) ? this->Text.substr((size_t)recSels, (size_t)(recSele - recSels)) : L"";
			rec.insertedText = L"";
			rec.selStartBefore = this->SelectionStart;
			rec.selEndBefore = this->SelectionEnd;
			shouldRecord = true;
		}
		std::vector<wchar_t> editBuffer = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
		for (int i = 0; i < selLen; i++)
		{
			editBuffer.erase(editBuffer.begin() + sels);
		}
		this->SelectionStart = this->SelectionEnd = sels;
		this->Text = BuildTextFromBuffer(editBuffer);
	}
	else
	{
		if (sels > 0)
		{
			if (!this->isApplyingUndoRedo)
			{
				int recPos = std::clamp(sels - 1, 0, (int)this->Text.size());
				rec.pos = recPos;
				rec.removedText = this->Text.substr((size_t)recPos, (size_t)1);
				rec.insertedText = L"";
				rec.selStartBefore = this->SelectionStart;
				rec.selEndBefore = this->SelectionEnd;
				shouldRecord = true;
			}
			std::vector<wchar_t> editBuffer = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
			editBuffer.erase(editBuffer.begin() + sels - 1);
			this->SelectionStart = this->SelectionEnd = sels - 1;
			this->Text = BuildTextFromBuffer(editBuffer);
		}
	}
	if (shouldRecord)
	{
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	this->OnTextChanged(this, oldStr, this->Text);
}
void TextBox::InputDelete()
{
	std::wstring oldStr = this->Text;
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int selLen = sele - sels;
	UndoRecord rec;
	bool shouldRecord = false;
	if (selLen > 0)
	{
		if (!this->isApplyingUndoRedo)
		{
			int recSels = std::clamp(sels, 0, (int)this->Text.size());
			int recSele = std::clamp(sele, 0, (int)this->Text.size());
			rec.pos = recSels;
			rec.removedText = (recSele > recSels) ? this->Text.substr((size_t)recSels, (size_t)(recSele - recSels)) : L"";
			rec.insertedText = L"";
			rec.selStartBefore = this->SelectionStart;
			rec.selEndBefore = this->SelectionEnd;
			shouldRecord = true;
		}
		std::vector<wchar_t> editBuffer = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
		for (int i = 0; i < selLen; i++)
		{
			editBuffer.erase(editBuffer.begin() + sels);
		}
		this->SelectionStart = this->SelectionEnd = sels;
		this->Text = BuildTextFromBuffer(editBuffer);
	}
	else
	{
		if (sels < static_cast<int>(this->Text.size()))
		{
			if (!this->isApplyingUndoRedo)
			{
				int recPos = std::clamp(sels, 0, (int)this->Text.size());
				rec.pos = recPos;
				rec.removedText = this->Text.substr((size_t)recPos, (size_t)1);
				rec.insertedText = L"";
				rec.selStartBefore = this->SelectionStart;
				rec.selEndBefore = this->SelectionEnd;
				shouldRecord = true;
			}
			std::vector<wchar_t> editBuffer = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
			editBuffer.erase(editBuffer.begin() + sels);
			this->SelectionStart = this->SelectionEnd = sels;
			this->Text = BuildTextFromBuffer(editBuffer);
		}
	}
	if (shouldRecord)
	{
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	this->OnTextChanged(this, oldStr, this->Text);
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
	this->Text = newText;
	this->OnTextChanged(this, oldStr, this->Text);
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
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	if (sele > sels)
	{
		std::wstring s = L"";
		for (int i = sels; i < sele; i++)
		{
			s += this->Text[i];
		}
		return s;
	}
	return L"";
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
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
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
						auto absoluteLocation = this->AbsLocation;
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
				auto absoluteLocation = this->AbsLocation;
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
				this->ParentForm->Selected = this;
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
		this->ParentForm->Selected = this;
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
				auto absoluteLocation = this->AbsLocation;
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
			if (this->SelectionEnd < textLength)
			{
				this->SelectionEnd = this->SelectionEnd + 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				{
					this->SelectionStart = this->SelectionEnd;
				}
				if (this->SelectionEnd > textLength)
				{
					this->SelectionEnd = textLength;
				}
				UpdateScroll();
			}
		}
		else if (wParam == VK_LEFT)
		{
			if (this->SelectionEnd > 0)
			{
				this->SelectionEnd = this->SelectionEnd - 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				{
					this->SelectionStart = this->SelectionEnd;
				}
				if (this->SelectionEnd < 0)
				{
					this->SelectionEnd = 0;
				}
				UpdateScroll();
			}
		}
		else if (wParam == VK_HOME)
		{
			auto font = this->Font;
			auto hit = font->HitTestTextRange(this->Text, (UINT32)this->SelectionEnd, (UINT32)0);
			this->SelectionEnd = font->HitTestTextPosition(this->Text, 0, hit[0].top + (font->FontHeight * 0.5f));
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd < 0)
			{
				this->SelectionEnd = 0;
			}
			UpdateScroll();
		}
		else if (wParam == VK_END)
		{
			auto font = this->Font;
			auto hit = font->HitTestTextRange(this->Text, (UINT32)this->SelectionEnd, (UINT32)0);
			this->SelectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX, hit[0].top + (font->FontHeight * 0.5f));
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			int textLength = static_cast<int>(this->Text.size());
			if (this->SelectionEnd > textLength)
			{
				this->SelectionEnd = textLength;
			}
			UpdateScroll();
		}
		else if (wParam == VK_PRIOR)
		{
			auto font = this->Font;
			auto hit = font->HitTestTextRange(this->Text, (UINT32)this->SelectionEnd, (UINT32)0);
			this->SelectionEnd = font->HitTestTextPosition(this->Text, hit[0].left, hit[0].top - this->Height);
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd < 0)
			{
				this->SelectionEnd = 0;
			}
			UpdateScroll(true);
		}
		else if (wParam == VK_NEXT)
		{
			auto font = this->Font;
			auto hit = font->HitTestTextRange(this->Text, (UINT32)this->SelectionEnd, (UINT32)0);
			this->SelectionEnd = font->HitTestTextPosition(this->Text, hit[0].left, hit[0].top + this->Height);
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			int textLength = static_cast<int>(this->Text.size());
			if (this->SelectionEnd > textLength)
			{
				this->SelectionEnd = textLength;
			}
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
		if (ch >= 32 && ch <= 126)
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
			if (OpenClipboard(this->ParentForm->Handle))
			{
				if (IsClipboardFormatAvailable(CF_UNICODETEXT))
				{
					HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
					const wchar_t* clipboardText = hClip ? (const wchar_t*)GlobalLock(hClip) : nullptr;
					if (clipboardText)
					{
						this->InputText(std::wstring(clipboardText));
						GlobalUnlock(hClip);
					}
					UpdateScroll();
					CloseClipboard();
				}
				else if (IsClipboardFormatAvailable(CF_TEXT))
				{
					HANDLE hClip = GetClipboardData(CF_TEXT);
					char* clipboardText = hClip ? (char*)GlobalLock(hClip) : nullptr;
					if (clipboardText)
					{
						int textLength = MultiByteToWideChar(CP_ACP, 0, clipboardText, static_cast<int>(strlen(clipboardText)), nullptr, 0);
						std::wstring wideText(textLength, L'\0');
						MultiByteToWideChar(CP_ACP, 0, clipboardText, static_cast<int>(strlen(clipboardText)), &wideText[0], textLength);
						GlobalUnlock(hClip);

						this->InputText(wideText);
					}
					UpdateScroll();
					CloseClipboard();
				}
				else
				{
					CloseClipboard();
				}
			}
		}
		else if (ch == 3)
		{
			std::wstring s = this->GetSelectedString();
			if (s.size() > 0 && OpenClipboard(this->ParentForm->Handle))
			{
				EmptyClipboard();
				const size_t bytes = (s.size() + 1) * sizeof(wchar_t);
				HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, bytes);
				wchar_t* pData = (wchar_t*)GlobalLock(hData);
				memcpy(pData, s.c_str(), bytes);
				GlobalUnlock(hData);
				SetClipboardData(CF_UNICODETEXT, hData);
				CloseClipboard();
			}
		}
		else if (ch == 24)
		{
			std::wstring s = this->GetSelectedString();
			if (s.size() > 0 && OpenClipboard(this->ParentForm->Handle))
			{
				EmptyClipboard();
				const size_t bytes = (s.size() + 1) * sizeof(wchar_t);
				HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, bytes);
				wchar_t* pData = (wchar_t*)GlobalLock(hData);
				memcpy(pData, s.c_str(), bytes);
				GlobalUnlock(hData);
				SetClipboardData(CF_UNICODETEXT, hData);
				CloseClipboard();
			}
			this->InputBack();
		}
		this->InvalidateVisual();
	}
	break;
	case WM_IME_COMPOSITION:
	{
		if (lParam & GCS_RESULTSTR)
		{
			HIMC imeContext = ImmGetContext(this->ParentForm->Handle);
			if (imeContext)
			{
				DWORD byteCount = ImmGetCompositionStringW(imeContext, GCS_RESULTSTR, nullptr, 0);
				byteCount += sizeof(WCHAR);
				const DWORD charCount = byteCount / sizeof(wchar_t);
				std::vector<wchar_t> input(charCount, L'\0');
				if (ImmGetCompositionStringW(imeContext, GCS_RESULTSTR, input.data(), byteCount) > 0)
				{

					std::vector<wchar_t> textBuffer;
					for (DWORD charIndex = 0; charIndex < charCount; charIndex++)
					{
						if (input[charIndex] > 0xFF)
						{
							textBuffer.push_back(input[charIndex]);
						}
						if (!input[charIndex]) break;
					}
					textBuffer.push_back(L'\0');
					this->InputText(textBuffer.data());
					UpdateScroll();
				}
				ImmReleaseContext(this->ParentForm->Handle, imeContext);
			}
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
