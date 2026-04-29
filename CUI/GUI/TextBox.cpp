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

	bool WriteClipboardText(HWND hwnd, const std::wstring& text)
	{
		if (text.empty() || !OpenClipboard(hwnd)) return false;
		EmptyClipboard();
		const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
		HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (!hData)
		{
			CloseClipboard();
			return false;
		}
		wchar_t* data = static_cast<wchar_t*>(GlobalLock(hData));
		if (!data)
		{
			GlobalFree(hData);
			CloseClipboard();
			return false;
		}
		memcpy(data, text.c_str(), bytes);
		GlobalUnlock(hData);
		if (!SetClipboardData(CF_UNICODETEXT, hData))
		{
			GlobalFree(hData);
			CloseClipboard();
			return false;
		}
		CloseClipboard();
		return true;
	}

	bool ReadClipboardText(HWND hwnd, std::wstring& text)
	{
		text.clear();
		if (!OpenClipboard(hwnd)) return false;
		if (IsClipboardFormatAvailable(CF_UNICODETEXT))
		{
			HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
			const wchar_t* data = static_cast<const wchar_t*>(GlobalLock(hClip));
			if (data)
			{
				text = data;
				GlobalUnlock(hClip);
			}
		}
		else if (IsClipboardFormatAvailable(CF_TEXT))
		{
			HANDLE hClip = GetClipboardData(CF_TEXT);
			const char* data = static_cast<const char*>(GlobalLock(hClip));
			if (data)
			{
				const int len = MultiByteToWideChar(CP_ACP, 0, data, -1, NULL, 0);
				if (len > 0)
				{
					std::wstring buffer(static_cast<size_t>(len), L'\0');
					MultiByteToWideChar(CP_ACP, 0, data, -1, &buffer[0], len);
					if (!buffer.empty() && buffer.back() == L'\0') buffer.pop_back();
					text = buffer;
				}
				GlobalUnlock(hClip);
			}
		}
		CloseClipboard();
		return !text.empty();
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
		std::vector<wchar_t> tmp = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
		if (sele > sels)
		{
			int sublen = sele - sels;
			for (int i = 0; i < sublen; i++)
			{
				tmp.erase(tmp.begin() + sels);
			}
			for (int i = 0; i < inputLength; i++)
			{
				tmp.insert(tmp.begin() + sels + i, input[i]);
			}
			SelectionEnd = SelectionStart = sels + inputLength;
			this->Text = BuildTextFromBuffer(tmp);
		}
		else if (sele == sels && sele >= 0)
		{
			for (int i = 0; i < inputLength; i++)
			{
				tmp.insert(tmp.begin() + sels + i, input[i]);
			}
			SelectionEnd += inputLength;
			SelectionStart += inputLength;
			this->Text = BuildTextFromBuffer(tmp);
		}
		else
		{
			this->Text += input;
			SelectionEnd = SelectionStart = static_cast<int>(this->Text.size());
		}
	}
	if(this->Text.empty())
	{
		this->Text = L"";
	}
	std::vector<wchar_t> tmp = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
	for (size_t i = 0; i < tmp.size(); i++)
	{
		if (tmp[i] == L'\r' || tmp[i] == L'\n')
		{
			tmp[i] = L' ';
		}
	}
	this->Text = BuildTextFromBuffer(tmp);
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
		std::vector<wchar_t> tmp = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
		for (int i = 0; i < selLen; i++)
		{
			tmp.erase(tmp.begin() + sels);
		}
		this->SelectionStart = this->SelectionEnd = sels;
		this->Text = BuildTextFromBuffer(tmp);
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
			std::vector<wchar_t> tmp = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
			tmp.erase(tmp.begin() + sels - 1);
			this->SelectionStart = this->SelectionEnd = sels - 1;
			this->Text = BuildTextFromBuffer(tmp);
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
		std::vector<wchar_t> tmp = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
		for (int i = 0; i < selLen; i++)
		{
			tmp.erase(tmp.begin() + sels);
		}
		this->SelectionStart = this->SelectionEnd = sels;
		this->Text = BuildTextFromBuffer(tmp);
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
			std::vector<wchar_t> tmp = std::vector<wchar_t>(this->_text.begin(), this->_text.end());
			tmp.erase(tmp.begin() + sels);
			this->SelectionStart = this->SelectionEnd = sels;
			this->Text = BuildTextFromBuffer(tmp);
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
	float render_width = this->Width - (TextMargin * 2.0f);
	auto font = this->Font;
	auto lastSelect = font->HitTestTextRange(this->Text, (UINT32)SelectionEnd, (UINT32)0)[0];
	if ((lastSelect.left + lastSelect.width) - OffsetX > render_width)
	{
		OffsetX = (lastSelect.left + lastSelect.width) - render_width;
	}
	if (lastSelect.left - OffsetX < 0.0f)
	{
		OffsetX = lastSelect.left;
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

int TextBox::SelectionLength() const
{
	return std::abs(this->SelectionEnd - this->SelectionStart);
}

bool TextBox::HasSelection() const
{
	return this->SelectionLength() > 0;
}

void TextBox::Select(int start, int length)
{
	const int textLength = static_cast<int>(this->Text.size());
	const int selStart = std::clamp(start, 0, textLength);
	const int selEnd = std::clamp(selStart + std::max(0, length), 0, textLength);
	this->SelectionStart = selStart;
	this->SelectionEnd = selEnd;
	this->UpdateScroll();
	this->PostRender();
}

void TextBox::SelectAll()
{
	this->Select(0, static_cast<int>(this->Text.size()));
}

void TextBox::ClearSelection()
{
	const int caret = std::clamp(std::max(this->SelectionStart, this->SelectionEnd), 0, static_cast<int>(this->Text.size()));
	this->SelectionStart = caret;
	this->SelectionEnd = caret;
	this->UpdateScroll();
	this->PostRender();
}

void TextBox::Clear()
{
	if (this->Text.empty()) return;
	this->SelectAll();
	this->InputBack();
	this->UpdateScroll();
	this->PostRender();
}

void TextBox::InsertText(std::wstring text)
{
	this->InputText(text);
	this->UpdateScroll();
	this->PostRender();
}

bool TextBox::Copy()
{
	const HWND hwnd = this->ParentForm ? this->ParentForm->Handle : NULL;
	return WriteClipboardText(hwnd, this->GetSelectedString());
}

bool TextBox::Cut()
{
	if (!this->HasSelection() || !this->Copy()) return false;
	this->InputBack();
	this->UpdateScroll();
	this->PostRender();
	return true;
}

bool TextBox::Paste()
{
	std::wstring text;
	const HWND hwnd = this->ParentForm ? this->ParentForm->Handle : NULL;
	if (!ReadClipboardText(hwnd, text)) return false;
	this->InputText(text);
	this->UpdateScroll();
	this->PostRender();
	return true;
}

void TextBox::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	float render_height = this->Height - (TextMargin * 2.0f);
	textSize = font->GetTextSize(this->Text, FLT_MAX, render_height);
	float OffsetY = (this->Height - textSize.height) * 0.5f;
	if (OffsetY < 0.0f)OffsetY = 0.0f;
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
		if (isUnderMouse || isSelected)
		{
			backColor.r = std::min(1.0f, backColor.r * 1.2f);
			backColor.g = std::min(1.0f, backColor.g * 1.2f);
			backColor.b = std::min(1.0f, backColor.b * 1.2f);
		}
		d2d->FillRect(0, 0, actualWidth, actualHeight, backColor);
		if (this->Image)
		{
			this->RenderImage();
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
							sr.left + TextMargin - OffsetX,
							sr.top + OffsetY,
							sr.width, sr.height,
							this->SelectedBackColor);
					}
				}
				else
				{
					if (!selRange.empty())
					{
						const auto caret = selRange[0];
						const float cx = caret.left + TextMargin - OffsetX;
						const float cy = caret.top + OffsetY;
						const float ch = caret.height > 0 ? caret.height : font->FontHeight;
						auto abs = this->AbsLocation;
						this->_caretRectCache = { static_cast<float>(abs.x) + cx - 2.0f, static_cast<float>(abs.y) + cy - 2.0f, static_cast<float>(abs.x) + cx + 2.0f, static_cast<float>(abs.y) + cy + ch + 2.0f };
						this->_caretRectCacheValid = true;
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + TextMargin - OffsetX, selRange[0].top + OffsetY };
						caretEnd = { selRange[0].left + TextMargin - OffsetX, selRange[0].top + selRange[0].height + OffsetY };
					}
				}
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				if (lot) {
					d2d->DrawStringLayoutEffect(lot,
						TextMargin - OffsetX, OffsetY,
						this->ForeColor,
						DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
						this->SelectedForeColor,
						font);
					lot->Release();
				}
			}
			else
			{
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				if (lot) {
					d2d->DrawStringLayout(lot,
						TextMargin - OffsetX, OffsetY,
						this->ForeColor);
					lot->Release();
				}
			}
		}
		else
		{
			if (isSelected)
			{
				const float cx = (float)TextMargin - OffsetX;
				const float cy = OffsetY;
				const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				auto abs = this->AbsLocation;
				this->_caretRectCache = { static_cast<float>(abs.x) + cx - 2.0f, static_cast<float>(abs.y) + cy - 2.0f, static_cast<float>(abs.x) + cx + 2.0f, static_cast<float>(abs.y) + cy + ch + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { (float)TextMargin - OffsetX, OffsetY };
				caretEnd = { (float)TextMargin - OffsetX, OffsetY + 16.0f };
			}
		}
		UpdateCaretBlinkState(isSelected, this->SelectionStart, this->SelectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->ForeColor);
		}
		d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BolderColor, this->Boder);
		if (!this->Enable)
		{
			d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

bool TextBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}
bool TextBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT i = 0; i < uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.push_back(strFileName);
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
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, event_obj);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && this->ParentForm->Selected == this)
		{
			auto font = this->Font;
			float render_height = this->Height - (TextMargin * 2.0f);
			SelectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX, render_height, (xof - TextMargin) + this->OffsetX, yof - TextMargin);
			UpdateScroll();
			this->PostRender();
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
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
				auto lse = this->ParentForm->Selected;
				this->ParentForm->Selected = this;
				if (lse) lse->PostRender();
			}
			auto font = this->Font;
			float render_height = this->Height - (TextMargin * 2.0f);
			this->SelectionStart = this->SelectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX, render_height, (xof - TextMargin) + this->OffsetX, yof - TextMargin);
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (this->ParentForm->Selected == this)
		{
			float render_height = this->Height - (TextMargin * 2.0f);
			auto font = this->Font;
			SelectionEnd = font->HitTestTextPosition(this->Text, FLT_MAX, render_height, (xof - TextMargin) + this->OffsetX, yof - TextMargin);
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		this->ParentForm->Selected = this;
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, event_obj);
		this->PostRender();
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
				this->PostRender();
				return true;
			}
			if (wParam == 'Y')
			{
				this->Redo();
				UpdateScroll();
				this->PostRender();
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
				auto abs = this->AbsLocation;
				float caretX = (float)abs.x + this->TextMargin - this->OffsetX;
				float caretY = (float)abs.y;
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
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
		this->PostRender();
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
					const wchar_t* pBuf = (const wchar_t*)GlobalLock(hClip);
					if (pBuf)
					{
						this->InputText(std::wstring(pBuf));
						GlobalUnlock(hClip);
					}
					UpdateScroll();
					CloseClipboard();
				}
				else if (IsClipboardFormatAvailable(CF_TEXT))
				{
					HANDLE hClip = GetClipboardData(CF_TEXT);
					char* pBuf = (char*)GlobalLock(hClip);
					if (pBuf)
					{
						GlobalUnlock(hClip);
						int len = MultiByteToWideChar(CP_ACP, 0, pBuf, static_cast<int>(strlen(pBuf)), NULL, 0);
						std::wstring wstr(len, L'\0');
						MultiByteToWideChar(CP_ACP, 0, pBuf, static_cast<int>(strlen(pBuf)), &wstr[0], len);

						this->InputText(wstr);
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
		this->PostRender();
	}
	break;
	case WM_IME_COMPOSITION:
	{
		if (lParam & GCS_RESULTSTR)
		{
			HIMC hIMC;
			DWORD dwSize;
			hIMC = ImmGetContext(this->ParentForm->Handle);
			dwSize = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
			dwSize += sizeof(WCHAR);
			wchar_t* input = new wchar_t[dwSize];
			memset(input, 0, dwSize);
			if (ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, input, dwSize) > 0)
			{

				std::vector<wchar_t> tmp;
				for (DWORD i = 0; i < dwSize - 2; i++)
				{
					if (input[i] > 0xFF)
					{
						tmp.push_back(input[i]);
					}
					if (!input[i]) break;
				}
				delete[] input;
				tmp.push_back(L'\0');
				this->InputText(tmp.data());
				UpdateScroll();
			}
			ImmReleaseContext(this->ParentForm->Handle, hIMC);
			this->PostRender();
		}
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, event_obj);
		this->PostRender();
	}
	break;
	}
	return true;
}
