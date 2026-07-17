#pragma once
#define NOMINMAX
#include "RichTextBox.h"
#include "Form.h"
#include "TextEditCore.h"
#include <algorithm>
#include <cstring>
#pragma comment(lib, "Imm32.lib")

namespace
{
	CuiTextEdit::EditOptions RichEditOptions(bool allowMultiLine)
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = allowMultiLine;
		return options;
	}

	void ApplyRichTextWrapping(IDWriteTextLayout* layout)
	{
		if (layout)
			layout->SetWordWrapping(DWRITE_WORD_WRAPPING_CHARACTER);
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

UIClass RichTextBox::Type() { return UIClass::UI_RichTextBox; }

bool RichTextBox::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	UpdateLayout();
	const float renderHeight = this->Height - (TextMargin * 2.0f);
	const float maxScroll = std::max(0.0f, textSize.height - renderHeight);
	if (renderHeight <= 0.0f || maxScroll <= 0.0f)
		return false;
	if (this->VerticalScrollOffset < 0.0f) this->VerticalScrollOffset = 0.0f;
	if (this->VerticalScrollOffset > maxScroll) this->VerticalScrollOffset = maxScroll;
	return delta > 0
		? this->VerticalScrollOffset > 0.0f
		: this->VerticalScrollOffset < maxScroll;
}

bool RichTextBox::HandlesNavigationKey(WPARAM key) const
{
	if (key == VK_TAB)
		return this->AllowTabInput;
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

CursorKind RichTextBox::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->Enable) return CursorKind::Arrow;

	const float renderHeight = (float)this->Height - (this->TextMargin * 2.0f);
	const bool hasVScroll = (renderHeight > 0.0f) && (this->textSize.height > renderHeight);
	if (hasVScroll && localX >= (this->Width - 8))
		return CursorKind::SizeNS;

	return CursorKind::IBeam;
}
RichTextBox::RichTextBox(std::wstring text, int x, int y, int width, int height)
{
	AllowMultiLine = true;
	this->Text = NormalizeLineBreaks(text);
	this->buffer = this->Text;
	this->bufferSyncedFromControl = true;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::BorderStrong;
	this->ForeColor = cui::theme::palette::TextPrimary;
	UpdateLayout();
}

void RichTextBox::SyncBufferFromControlIfNeeded()
{
	if (!this->bufferSyncedFromControl || this->TextChanged)
	{
		this->buffer = this->AllowMultiLine ? NormalizeLineBreaks(this->Text) : this->Text;
		this->bufferSyncedFromControl = true;
	}
}

std::wstring RichTextBox::NormalizeLineBreaks(const std::wstring& text) const
{
	return CuiTextEdit::NormalizeInput(text, RichEditOptions(this->AllowMultiLine));
}

bool RichTextBox::HasCrLfAt(int index) const
{
	return CuiTextEdit::HasCrLfAt(this->buffer, index);
}

bool RichTextBox::IsCaretBetweenCrLf(int index) const
{
	return CuiTextEdit::IsBetweenCrLf(this->buffer, index);
}

int RichTextBox::GetNextCaretIndex(int index) const
{
	return CuiTextEdit::GetNextCaretIndex(this->buffer, index, this->AllowMultiLine);
}

int RichTextBox::GetPreviousCaretIndex(int index) const
{
	return CuiTextEdit::GetPreviousCaretIndex(this->buffer, index, this->AllowMultiLine);
}

void RichTextBox::NormalizeSelectionRangeForErase(int& start, int& end) const
{
	CuiTextEdit::NormalizeSelectionForTextElements(this->buffer, start, end, this->AllowMultiLine);
}

bool RichTextBox::GetBackspaceEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const
{
	return CuiTextEdit::GetBackspaceEraseRange(this->buffer, caretIndex, this->AllowMultiLine, eraseStart, eraseLength);
}

bool RichTextBox::GetDeleteEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const
{
	return CuiTextEdit::GetDeleteEraseRange(this->buffer, caretIndex, this->AllowMultiLine, eraseStart, eraseLength);
}

void RichTextBox::SyncControlTextFromBuffer(const std::wstring& oldText)
{
	if (oldText == this->buffer)
		return;
	this->SetTextInternal(this->buffer);
	this->TextChanged = true;
	this->OnTextChanged(this, oldText, this->buffer);
}

void RichTextBox::TrimToMaxLength()
{
	if (this->MaxTextLength == 0) return;
	if (this->buffer.size() <= this->MaxTextLength) return;

	const size_t removeCount = this->buffer.size() - this->MaxTextLength;
	if (removeCount == 0) return;

	this->buffer = this->buffer.substr(removeCount);

	this->SelectionStart = std::max(0, this->SelectionStart - (int)removeCount);
	this->SelectionEnd = std::max(0, this->SelectionEnd - (int)removeCount);
	if (this->SelectionStart > (int)this->buffer.size()) this->SelectionStart = (int)this->buffer.size();
	if (this->SelectionEnd > (int)this->buffer.size()) this->SelectionEnd = (int)this->buffer.size();
}

void RichTextBox::UpdateSelRange()
{
	if (!this->_textLayoutCache)
		return;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int selLen = sele - sels;
	selRange = font->HitTestTextRange(this->_textLayoutCache, (UINT32)sels, (UINT32)selLen);

	this->_textLayoutCache->SetDrawingEffect(nullptr, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	this->_textLayoutCache->SetDrawingEffect(d2d->GetBackColorBrush(this->SelectedForeColor), DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen });
	this->selRangeDirty = false;
}
void RichTextBox::UpdateLayout()
{
	auto font = this->Font;
	if (font != this->_lastLayoutFont)
	{
		this->_lastLayoutFont = font;
		this->TextChanged = true;
		this->selRangeDirty = true;
		this->blocksDirty = true;
		this->blockMetricsDirty = true;
		this->_caretRectCacheValid = false;
		if (this->_textLayoutCache)
		{
			this->_textLayoutCache->Release();
			this->_textLayoutCache = nullptr;
		}
		ReleaseBlocks();
	}

	if (!this->ParentForm)
		return;
	SyncBufferFromControlIfNeeded();

	this->_isVirtualized = (this->EnableVirtualization && this->AllowMultiLine && this->buffer.size() >= this->VirtualizeThreshold);
	if (this->_isVirtualized)
	{
		if (this->_textLayoutCache)
		{
			this->_textLayoutCache->Release();
			this->_textLayoutCache = nullptr;
		}

		float renderWidth = this->Width - (TextMargin * 2.0f);
		float renderHeight = this->Height - (TextMargin * 2.0f);

		if (this->TextChanged || this->lastLayoutSize.cx != this->Width || this->lastLayoutSize.cy != this->Height || this->blocksDirty)
		{
			RebuildBlocks();
			this->lastLayoutSize = SIZE{ this->Width, this->Height };
			this->TextChanged = false;
		}

		EnsureAllBlockMetrics(renderWidth, renderHeight);
		this->textSize.height = this->virtualTotalHeight;
		this->textSize.width = renderWidth;
		this->selRangeDirty = true;
		return;
	}

	ReleaseBlocks();

	if ((this->TextChanged || this->lastLayoutSize.cx != this->Width || this->lastLayoutSize.cy != this->Height) && this->ParentForm)
	{
		if (this->_textLayoutCache)this->_textLayoutCache->Release();
		auto d2d = this->ParentForm->Render;
		if (d2d)
		{
			auto font = this->Font;
			float renderWidth = this->Width - (TextMargin * 2.0f);
			float renderHeight = this->Height - (TextMargin * 2.0f);

			this->_textLayoutCache = d2d->CreateStringLayout(this->buffer, renderWidth, renderHeight, font);
			ApplyRichTextWrapping(this->_textLayoutCache);
			textSize = font->GetTextSize(_textLayoutCache);
			if (textSize.height > renderHeight)
			{
				if (this->_textLayoutCache) this->_textLayoutCache->Release();
				this->_textLayoutCache = d2d->CreateStringLayout(this->buffer, renderWidth - 8.0f, renderHeight, font);
				ApplyRichTextWrapping(this->_textLayoutCache);
				textSize = font->GetTextSize(_textLayoutCache);
			}
			if (this->_textLayoutCache)
			{
				TextChanged = false;
				this->lastLayoutSize = SIZE{ this->Width, this->Height };
				this->selRangeDirty = true;
			}
		}
	}
}

void RichTextBox::ReleaseBlocks()
{
	for (auto& b : this->blocks)
	{
		if (b.layout)
		{
			b.layout->Release();
			b.layout = nullptr;
		}
	}
	this->blocks.clear();
	this->blockTops.clear();
	this->blocksDirty = true;
	this->blockMetricsDirty = true;
	this->virtualTotalHeight = 0.0f;
	this->layoutWidthHasScrollBar = false;
	this->_cachedRenderWidth = 0.0f;
}

void RichTextBox::RebuildBlocks()
{
	ReleaseBlocks();
	this->blocksDirty = false;
	this->blockMetricsDirty = true;

	const size_t bufferLength = this->buffer.size();
	if (bufferLength == 0) return;

	const size_t blockSize = (std::max)((size_t)256, this->BlockCharCount);
	size_t blockStart = 0;
	while (blockStart < bufferLength)
	{
		size_t blockLength = (std::min)(blockSize, bufferLength - blockStart);
		if (blockStart + blockLength < bufferLength)
		{
			wchar_t last = this->buffer[blockStart + blockLength - 1];
			wchar_t next = this->buffer[blockStart + blockLength];
			bool lastHigh = (last >= 0xD800 && last <= 0xDBFF);
			bool nextLow = (next >= 0xDC00 && next <= 0xDFFF);
			if (lastHigh && nextLow)
			{
				blockLength += 1;
			}
		}
		TextBlock block;
		block.start = blockStart;
		block.len = blockLength;
		this->blocks.push_back(block);
		blockStart += blockLength;
	}
}

void RichTextBox::EnsureBlockLayout(int blockIndex, float renderWidth, float renderHeight)
{
	if (blockIndex < 0 || blockIndex >= (int)this->blocks.size()) return;
	auto& block = this->blocks[blockIndex];
	if (block.layout && block.height >= 0.0f) return;

	auto d2d = this->ParentForm->Render;
	auto font = this->Font;

	std::wstring blockText = this->buffer.substr(block.start, block.len);
	block.layout = d2d->CreateStringLayout(blockText, renderWidth, FLT_MAX, font);
	ApplyRichTextWrapping(block.layout);
	auto blockSize = font->GetTextSize(block.layout);
	block.height = blockSize.height;
	if (block.height < font->FontHeight) block.height = font->FontHeight;
}

void RichTextBox::EnsureAllBlockMetrics(float renderWidth, float renderHeight)
{
	if (!this->blockMetricsDirty && this->_cachedRenderWidth == renderWidth)
		return;

	this->_cachedRenderWidth = renderWidth;
	this->virtualTotalHeight = 0.0f;
	this->blockTops.resize(this->blocks.size());

	auto computeTotalHeight = [&](float layoutWidth) {
		for (auto& block : this->blocks)
		{
			if (block.layout)
			{
				block.layout->Release();
				block.layout = nullptr;
			}
			block.height = -1.0f;
		}
		float blockTop = 0.0f;
		for (int i = 0; i < (int)this->blocks.size(); i++)
		{
			this->blockTops[i] = blockTop;
			EnsureBlockLayout(i, layoutWidth, renderHeight);
			blockTop += this->blocks[i].height;
		}
		return blockTop;
		};

	float totalHeight = computeTotalHeight(renderWidth);
	bool needsScrollBar = totalHeight > renderHeight;
	if (needsScrollBar)
	{
		totalHeight = computeTotalHeight(std::max(0.0f, renderWidth - 8.0f));
		this->layoutWidthHasScrollBar = true;
	}
	else
	{
		this->layoutWidthHasScrollBar = false;
	}
	this->virtualTotalHeight = totalHeight;
	this->blockMetricsDirty = false;
}

int RichTextBox::HitTestGlobalIndex(float x, float y)
{
	if (!this->_isVirtualized || this->blocks.empty()) return 0;
	float renderHeight = this->Height - (TextMargin * 2.0f);
	float renderWidth = this->Width - (TextMargin * 2.0f);
	if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

	float contentY = (y + this->VerticalScrollOffset) - this->TextMargin;
	if (contentY < 0) contentY = 0;

	int blockIndex = 0;
	for (int i = 0; i < (int)this->blockTops.size(); i++)
	{
		if (contentY >= this->blockTops[i])
			blockIndex = i;
		else
			break;
	}
	EnsureBlockLayout(blockIndex, renderWidth, renderHeight);
	float yInBlock = contentY - this->blockTops[blockIndex];
	float xInBlock = x - this->TextMargin;
	if (xInBlock < 0) xInBlock = 0;

	int localIndex = this->Font->HitTestTextPosition(this->blocks[blockIndex].layout, xInBlock, yInBlock);
	int globalIndex = (int)this->blocks[blockIndex].start + localIndex;
	globalIndex = std::clamp(globalIndex, 0, (int)this->buffer.size());
	return globalIndex;
}

bool RichTextBox::GetCaretMetrics(int caretIndex, float& outX, float& outY, float& outH)
{
	outX = outY = outH = 0.0f;
	if (!this->_isVirtualized || this->blocks.empty()) return false;

	float renderHeight = this->Height - (TextMargin * 2.0f);
	float renderWidth = this->Width - (TextMargin * 2.0f);
	if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

	caretIndex = std::clamp(caretIndex, 0, (int)this->buffer.size());
	int blockIndex = 0;
	for (int i = 0; i < (int)this->blocks.size(); i++)
	{
		if (caretIndex >= (int)this->blocks[i].start && caretIndex <= (int)(this->blocks[i].start + this->blocks[i].len))
		{
			blockIndex = i;
			break;
		}
	}
	EnsureBlockLayout(blockIndex, renderWidth, renderHeight);
	int localIndex = caretIndex - (int)this->blocks[blockIndex].start;
	auto hit = this->Font->HitTestTextRange(this->blocks[blockIndex].layout, (UINT32)localIndex, (UINT32)0);
	if (hit.empty()) return false;
	outX = hit[0].left + this->TextMargin;
	outY = (this->blockTops[blockIndex] + hit[0].top) - this->VerticalScrollOffset + this->TextMargin;
	outH = hit[0].height;
	return true;
}
void RichTextBox::DrawScroll()
{
	auto d2d = this->ParentForm->Render;
	float renderHeight = this->Height - (TextMargin * 2.0f);
	float maxScroll = textSize.height - renderHeight;
	if (this->VerticalScrollOffset > maxScroll)
	{
		this->VerticalScrollOffset = maxScroll;
		if (this->VerticalScrollOffset < 0)this->VerticalScrollOffset = 0;
	}
	if (textSize.height > renderHeight)
	{
		float scrollThumbHeight = (renderHeight / textSize.height) * renderHeight;
		if (scrollThumbHeight < this->Height * 0.1f)scrollThumbHeight = this->Height * 0.1f;
		float scrollThumbMoveSpace = this->Height - scrollThumbHeight;
		float scrollRatio = (float)this->VerticalScrollOffset / (float)maxScroll;
		float scrollThumbTop = scrollRatio * scrollThumbMoveSpace;
		// 局部坐标：滚动条 X = Width - 8，Y = 0
		d2d->FillRoundRect(this->Width - 8.0f, 0, 8.0f, static_cast<float>(this->Height), this->ScrollBackColor, 4.0f);
		d2d->FillRoundRect(this->Width - 8.0f, scrollThumbTop, 8.0f, scrollThumbHeight, this->ScrollForeColor, 4.0f);
	}
}

void RichTextBox::ScrollToEnd()
{
	this->UpdateLayout();
	float renderHeight = this->Height - (TextMargin * 2.0f);
	float maxScroll = textSize.height - renderHeight;
	this->VerticalScrollOffset = maxScroll;
	if (this->VerticalScrollOffset < 0)this->VerticalScrollOffset = 0;
	this->SelectionEnd = this->SelectionStart = (int)this->buffer.size();
	this->InvalidateVisual();
}
void RichTextBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;

	float renderHeight = this->Height - (TextMargin * 2.0f);
	float maxScroll = textSize.height - renderHeight;

	float scrollBlockHeight = (renderHeight / textSize.height) * renderHeight;
	if (scrollBlockHeight < this->Height * 0.1f)scrollBlockHeight = this->Height * 0.1f;

	float scrollHeight = this->Height - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float thumbGrabOffset = std::clamp(_verticalScrollThumbGrabOffset, 0.0f, scrollBlockHeight);
	float targetTop = posY - thumbGrabOffset;
	float scrollRatio = targetTop / scrollHeight;
	scrollRatio = std::clamp(scrollRatio, 0.0f, 1.0f);
	float newScroll = scrollRatio * maxScroll;
	{
		this->VerticalScrollOffset = newScroll;
		if (this->VerticalScrollOffset < 0) this->VerticalScrollOffset = 0;
		if (this->VerticalScrollOffset > maxScroll + 1) this->VerticalScrollOffset = maxScroll + 1;
		InvalidateVisual();
	}
}
void RichTextBox::SetScrollByPos(float localY)
{
	const float renderHeight = this->Height - (TextMargin * 2.0f);
	if (renderHeight <= 0.0f || textSize.height <= 0.0f)
	{
		this->VerticalScrollOffset = 0.0f;
		return;
	}

	if (textSize.height <= renderHeight)
	{
		this->VerticalScrollOffset = 0.0f;
		return;
	}

	const float maxScroll = std::max(0.0f, textSize.height - renderHeight);

	float scrollBlockHeight = (renderHeight / textSize.height) * renderHeight;
	if (scrollBlockHeight < this->Height * 0.1f) scrollBlockHeight = this->Height * 0.1f;
	if (scrollBlockHeight > static_cast<float>(this->Height)) scrollBlockHeight = static_cast<float>(this->Height);

	const float topPosition = scrollBlockHeight * 0.5f;
	const float bottomPosition = this->Height - topPosition;
	if (bottomPosition > topPosition)
	{
		const float percent = std::clamp((localY - topPosition) / (bottomPosition - topPosition), 0.0f, 1.0f);
		this->VerticalScrollOffset = maxScroll * percent;
	}
	this->VerticalScrollOffset = std::clamp(this->VerticalScrollOffset, 0.0f, maxScroll);
}
void RichTextBox::InputText(std::wstring input)
{
	SyncBufferFromControlIfNeeded();
	TrimToMaxLength();
	std::wstring oldText = this->buffer;
	const int selStartBefore = this->SelectionStart;
	const int selEndBefore = this->SelectionEnd;
	auto result = CuiTextEdit::ReplaceSelection(this->buffer, this->SelectionStart, this->SelectionEnd, input, RichEditOptions(this->AllowMultiLine));
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = result.insertedText;
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
	}
	TrimToMaxLength();
	this->selRangeDirty = true;
	this->blocksDirty = true;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	SyncControlTextFromBuffer(oldText);
}
void RichTextBox::InputBack()
{
	SyncBufferFromControlIfNeeded();
	std::wstring oldText = this->buffer;
	const int selStartBefore = this->SelectionStart;
	const int selEndBefore = this->SelectionEnd;
	auto result = CuiTextEdit::Backspace(this->buffer, this->SelectionStart, this->SelectionEnd, RichEditOptions(this->AllowMultiLine));
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
	}
	this->selRangeDirty = true;
	this->blocksDirty = true;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	SyncControlTextFromBuffer(oldText);
}
void RichTextBox::InputDelete()
{
	SyncBufferFromControlIfNeeded();
	std::wstring oldText = this->buffer;
	const int selStartBefore = this->SelectionStart;
	const int selEndBefore = this->SelectionEnd;
	auto result = CuiTextEdit::DeleteForward(this->buffer, this->SelectionStart, this->SelectionEnd, RichEditOptions(this->AllowMultiLine));
	UndoRecord rec;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
	}
	this->selRangeDirty = true;
	this->blocksDirty = true;
	if (result.textChanged && !this->isApplyingUndoRedo)
	{
		rec.selStartAfter = this->SelectionStart;
		rec.selEndAfter = this->SelectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	SyncControlTextFromBuffer(oldText);
}
void RichTextBox::ApplyUndoRecord(const UndoRecord& rec, bool isUndo)
{
	SyncBufferFromControlIfNeeded();
	std::wstring oldText = this->buffer;
	this->isApplyingUndoRedo = true;

	int pos = std::clamp(rec.pos, 0, (int)this->buffer.size());
	const std::wstring& removeText = isUndo ? rec.insertedText : rec.removedText;
	const std::wstring& insertText = isUndo ? rec.removedText : rec.insertedText;

	if (!removeText.empty() && pos <= (int)this->buffer.size())
	{
		size_t removeLen = std::min(removeText.size(), this->buffer.size() - (size_t)pos);
		this->buffer.erase((size_t)pos, removeLen);
	}
	if (!insertText.empty())
	{
		this->buffer.insert((size_t)pos, insertText);
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
	TrimToMaxLength();
	this->SelectionStart = std::clamp(this->SelectionStart, 0, (int)this->buffer.size());
	this->SelectionEnd = std::clamp(this->SelectionEnd, 0, (int)this->buffer.size());
	this->selRangeDirty = true;
	this->blocksDirty = true;

	this->isApplyingUndoRedo = false;
	SyncControlTextFromBuffer(oldText);
}
void RichTextBox::Undo()
{
	if (this->undoStack.empty()) return;
	UndoRecord rec = this->undoStack.back();
	this->undoStack.pop_back();
	ApplyUndoRecord(rec, true);
	this->redoStack.push_back(rec);
}
void RichTextBox::Redo()
{
	if (this->redoStack.empty()) return;
	UndoRecord rec = this->redoStack.back();
	this->redoStack.pop_back();
	ApplyUndoRecord(rec, false);
	this->undoStack.push_back(rec);
}
void RichTextBox::UpdateScroll(bool arrival)
{
	if (this->TextChanged || (this->_isVirtualized && (this->blocksDirty || this->blockMetricsDirty)) || (!this->_isVirtualized && this->_textLayoutCache == nullptr))
	{
		this->UpdateLayout();
	}

	if (this->_isVirtualized)
	{
		float cx, cy, ch;
		if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
		{
			float renderHeight = this->Height - (TextMargin * 2.0f);
			float caretTopContent = (cy - this->TextMargin) + this->VerticalScrollOffset;
			float caretBottomContent = caretTopContent + ch;
			if (arrival && this->SelectionEnd >= (int)this->buffer.size())
			{
				const float maxScroll = std::max(0.0f, this->textSize.height - renderHeight);
				this->VerticalScrollOffset = maxScroll;
			}
			else if (caretBottomContent - this->VerticalScrollOffset > renderHeight)
			{
				this->VerticalScrollOffset = caretBottomContent - renderHeight;
			}
			if (caretTopContent - this->VerticalScrollOffset < 0.0f)
				this->VerticalScrollOffset = caretTopContent;
			if (this->VerticalScrollOffset < 0) this->VerticalScrollOffset = 0;
		}
		return;
	}
	float renderHeight = this->Height - (TextMargin * 2.0f);
	auto font = this->Font;
	auto selected = font->HitTestTextRange(this->_textLayoutCache, (UINT32)SelectionEnd, (UINT32)0);
	if (selected.size() > 0)
	{
		auto lastSelect = selected[0];
		if (arrival && this->SelectionEnd >= (int)this->buffer.size())
		{
			const float maxScroll = std::max(0.0f, this->textSize.height - renderHeight);
			VerticalScrollOffset = maxScroll;
		}
		else if ((lastSelect.top + lastSelect.height) - VerticalScrollOffset > renderHeight)
		{
			VerticalScrollOffset = (lastSelect.top + lastSelect.height) - renderHeight;
		}
		if (lastSelect.top - VerticalScrollOffset < 0.0f)
		{
			VerticalScrollOffset = lastSelect.top;
		}
	}
}
void RichTextBox::AppendText(std::wstring str)
{
	SyncBufferFromControlIfNeeded();
	this->SelectionStart = this->SelectionEnd = (int)this->buffer.size();
	this->InputText(str);
	this->selRangeDirty = true;
}
void RichTextBox::AppendLine(std::wstring str)
{
	SyncBufferFromControlIfNeeded();
	this->SelectionStart = this->SelectionEnd = (int)this->buffer.size();
	this->InputText(str + L"\r\n");
	this->selRangeDirty = true;
}
std::wstring RichTextBox::GetSelectedString()
{
	SyncBufferFromControlIfNeeded();
	auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->buffer.size());
	if (!span.HasSelection())
		return L"";
	return this->buffer.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}

// ---- 公共选择/编辑 API ----
int RichTextBox::GetSelectionLength()
{
	auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->buffer.size());
	return span.HasSelection() ? static_cast<int>(span.Length()) : 0;
}

bool RichTextBox::HasSelection()
{
	return GetSelectionLength() > 0;
}

void RichTextBox::Select(int start, int length)
{
	SyncBufferFromControlIfNeeded();
	const int textLen = static_cast<int>(this->buffer.size());
	start = (std::clamp)(start, 0, textLen);
	length = (std::clamp)(length, 0, textLen - start);
	this->SelectionStart = start;
	this->SelectionEnd = start + length;
	this->selRangeDirty = true;
	this->InvalidateVisual();
}

void RichTextBox::SelectAll()
{
	Select(0, static_cast<int>(this->Text.size()));
}

void RichTextBox::ClearSelection()
{
	this->SelectionEnd = this->SelectionStart;
	this->selRangeDirty = true;
	this->InvalidateVisual();
}

void RichTextBox::Clear()
{
	if (this->ReadOnly) return;
	this->SelectAll();
	this->InputBack();
}

void RichTextBox::InsertText(const std::wstring& text)
{
	if (this->ReadOnly || text.empty()) return;
	this->InputText(text);
}

bool RichTextBox::Copy()
{
	const std::wstring selected = this->GetSelectedString();
	if (selected.empty()) return false;
	return WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, selected);
}

bool RichTextBox::Cut()
{
	if (this->ReadOnly) return false;
	const std::wstring selected = this->GetSelectedString();
	if (selected.empty()) return false;
	if (!WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, selected))
		return false;
	this->InputBack();
	return true;
}

bool RichTextBox::Paste()
{
	if (this->ReadOnly) return false;
	std::wstring clipboardText;
	if (!TryReadClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, clipboardText))
		return false;
	if (clipboardText.empty()) return false;
	this->InputText(clipboardText);
	return true;
}

void RichTextBox::Update()
{
	if (this->IsVisual == false)return;
	this->UpdateLayout();
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
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
		if (this->buffer.size() > 0)
		{
			if (this->_isVirtualized)
			{
				float renderWidth = this->Width - (TextMargin * 2.0f);
				float renderHeight = this->Height - (TextMargin * 2.0f);
				if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

				int sels = std::min(SelectionStart, SelectionEnd);
				int sele = std::max(SelectionStart, SelectionEnd);
				int selLen = sele - sels;

				float cx, cy, ch;
				if (isSelected && selLen == 0 && GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
				{
					selectedPos = { (int)(cx), (int)(cy) };
					{
						const float ah = (ch > 0.0f) ? ch : font->FontHeight;
						const auto absoluteLocation = this->GetAbsoluteLocationDip();
						this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ah + 2.0f };
						this->_caretRectCacheValid = true;
					}
					shouldDrawCaret = true;
					caretStart = { cx, cy };
					caretEnd = { cx, cy + ch };
				}

				float viewTop = this->VerticalScrollOffset;
				float viewBottom = this->VerticalScrollOffset + renderHeight;

				int first = 0;
				for (int i = 0; i < (int)this->blockTops.size(); i++)
				{
					if (this->blockTops[i] + this->blocks[i].height >= viewTop)
					{
						first = i;
						break;
					}
				}

				for (int i = first; i < (int)this->blocks.size(); i++)
				{
					float top = this->blockTops[i];
					float bottom = top + this->blocks[i].height;
					if (top > viewBottom) break;

					EnsureBlockLayout(i, renderWidth, renderHeight);
					float drawY = TextMargin + (top - this->VerticalScrollOffset);
					float drawX = TextMargin;

					if (isSelected && selLen != 0)
					{
						int blockStart = (int)this->blocks[i].start;
						int blockEnd = (int)(this->blocks[i].start + this->blocks[i].len);
						int is = std::max(sels, blockStart);
						int ie = std::min(sele, blockEnd);
						if (ie > is)
						{
							int localStart = is - blockStart;
							int localLen = ie - is;
							auto ranges = font->HitTestTextRange(this->blocks[i].layout, (UINT32)localStart, (UINT32)localLen);
							for (auto r : ranges)
							{
								d2d->FillRect(
									r.left + drawX,
									r.top + drawY,
									r.width,
									r.height,
									this->SelectedBackColor);
							}
						}
					}

					d2d->DrawStringLayout(this->blocks[i].layout, drawX, drawY, this->ForeColor);
				}
			}
			else if (isSelected)
			{
				if (isSelected && this->selRangeDirty)
				{
					UpdateSelRange();
				}
				int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
				int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
				int selLen = sele - sels;
				if (selLen != 0)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(
							sr.left + TextMargin,
							(sr.top + TextMargin) - this->VerticalScrollOffset,
							sr.width,
							sr.height,
							this->SelectedBackColor);
					}
				}
				else
				{
					if (selLen == 0 && !selRange.empty())
					{
						const auto caret = selRange[0];
						const float lx = caret.left + TextMargin;
						const float ly = (caret.top + TextMargin) - this->VerticalScrollOffset;
						const float ah = caret.height > 0 ? caret.height : font->FontHeight;
						const auto absoluteLocation = this->GetAbsoluteLocationDip();
						this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + lx - 2.0f, static_cast<float>(absoluteLocation.y) + ly - 2.0f, static_cast<float>(absoluteLocation.x) + lx + 2.0f, static_cast<float>(absoluteLocation.y) + ly + ah + 2.0f };
						this->_caretRectCacheValid = true;
					}
					if (!selRange.empty())
					{
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + TextMargin, (selRange[0].top + TextMargin) - this->VerticalScrollOffset };
						caretEnd = { selRange[0].left + TextMargin, (selRange[0].top + selRange[0].height + TextMargin) - this->VerticalScrollOffset };
					}
				}
				if (!selRange.empty())
				{
					selectedPos = { (int)selRange[0].left , (int)selRange[0].top };
					selectedPos.y -= static_cast<LONG>(this->VerticalScrollOffset);
					selectedPos.y += static_cast<LONG>(this->TextMargin);
					selectedPos.x += static_cast<LONG>(this->TextMargin);
				}
				d2d->DrawStringLayout(this->_textLayoutCache,
					TextMargin, TextMargin - this->VerticalScrollOffset,
					this->ForeColor);
			}
			else
			{
				d2d->DrawStringLayout(this->_textLayoutCache,
					TextMargin, TextMargin - this->VerticalScrollOffset,
					this->ForeColor);
			}
		}
		else
		{
			if (isSelected)
			{
				const float lx = (float)TextMargin;
				const float ly = 0.0f;
				const float ah = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				const auto absoluteLocation = this->GetAbsoluteLocationDip();
				this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + lx - 2.0f, static_cast<float>(absoluteLocation.y) + ly - 2.0f, static_cast<float>(absoluteLocation.x) + lx + 2.0f, static_cast<float>(absoluteLocation.y) + ly + ah + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { lx, ly };
				caretEnd = { lx, ly + 16.0f };
			}
		}
		UpdateCaretBlinkState(isSelected, this->SelectionStart, this->SelectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->ForeColor);
		}
		this->DrawScroll();
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

bool RichTextBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}
bool RichTextBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
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
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
		{
			if (this->VerticalScrollOffset > 0)
			{
				this->VerticalScrollOffset -= 10;
				if (this->VerticalScrollOffset < 0)this->VerticalScrollOffset = 0;
				this->InvalidateVisual();
			}
		}
		else
		{
			auto font = this->Font;
			float renderWidth = this->Width - (TextMargin * 2.0f);
			float renderHeight = this->Height - (TextMargin * 2.0f);
			if (textSize.height > renderHeight) renderWidth -= 8.0f;
			if (this->VerticalScrollOffset < textSize.height - renderHeight)
			{
				this->VerticalScrollOffset += 10;
				if (this->VerticalScrollOffset > textSize.height - renderHeight) this->VerticalScrollOffset = textSize.height - renderHeight;
				this->InvalidateVisual();
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (isDraggingScroll) {
			UpdateScrollDrag(static_cast<float>(localY));
		}
		if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && this->ParentForm->Selected == this && !isDraggingScroll)
		{
			auto font = this->Font;
			if (this->_isVirtualized)
				SelectionEnd = HitTestGlobalIndex((float)localX, (float)localY);
			else
				SelectionEnd = font->HitTestTextPosition(this->_textLayoutCache, localX - TextMargin, (localY + this->VerticalScrollOffset) - TextMargin);
			UpdateScroll();
			this->InvalidateVisual();
			this->selRangeDirty = true;
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
			if (localX >= Width - 8 && localX <= Width)
			{
				// 竖向滚动条：点在滑块上则用按下点锚定；否则用滑块中心（原行为）
				const float renderHeight = this->Height - (TextMargin * 2.0f);
				if (renderHeight > 0.0f && textSize.height > renderHeight)
				{
					const float maxScroll = std::max(0.0f, textSize.height - renderHeight);
					float thumbHeight = (renderHeight / textSize.height) * renderHeight;
					if (thumbHeight < this->Height * 0.1f) thumbHeight = this->Height * 0.1f;
					if (thumbHeight > static_cast<float>(this->Height)) thumbHeight = static_cast<float>(this->Height);
					const float moveSpace = std::max(0.0f, (float)this->Height - thumbHeight);
					float scrollRatio = 0.0f;
					if (maxScroll > 0.0f) scrollRatio = std::clamp(this->VerticalScrollOffset / maxScroll, 0.0f, 1.0f);
					const float thumbTop = scrollRatio * moveSpace;
					const float pointerY = (float)localY;
					const bool hitThumb = (pointerY >= thumbTop && pointerY <= (thumbTop + thumbHeight));
					_verticalScrollThumbGrabOffset = hitThumb ? (pointerY - thumbTop) : (thumbHeight * 0.5f);
				}
				else
				{
					_verticalScrollThumbGrabOffset = 0.0f;
				}
				isDraggingScroll = true;
				UpdateScrollDrag((float)localY);
				this->InvalidateVisual();
			}
			else
			{
				auto font = this->Font;
				if (this->_isVirtualized)
					this->SelectionStart = this->SelectionEnd = HitTestGlobalIndex((float)localX, (float)localY);
				else
					this->SelectionStart = this->SelectionEnd = font->HitTestTextPosition(this->_textLayoutCache, localX - TextMargin, (localY + this->VerticalScrollOffset) - TextMargin);
				this->selRangeDirty = true;
			}
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
		if (isDraggingScroll) {
			isDraggingScroll = false;
		}
		else if (this->ParentForm->Selected == this)
		{
			auto font = this->Font;
			if (this->_isVirtualized)
				SelectionEnd = HitTestGlobalIndex((float)localX, (float)localY);
			else
				SelectionEnd = font->HitTestTextPosition(this->_textLayoutCache, localX - TextMargin, (localY + this->VerticalScrollOffset) - TextMargin);
			this->selRangeDirty = true;
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		this->ParentForm->SetSelectedControl(this, false);
		SyncBufferFromControlIfNeeded();
		UpdateLayout();
		int hitIndex = 0;
		if (this->_isVirtualized)
			hitIndex = HitTestGlobalIndex((float)localX, (float)localY);
		else
			hitIndex = this->Font->HitTestTextPosition(this->_textLayoutCache, localX - TextMargin, (localY + this->VerticalScrollOffset) - TextMargin);
		hitIndex = std::clamp(hitIndex, 0, (int)this->buffer.size());
		this->SelectionStart = CuiTextEdit::GetLineStartIndex(this->buffer, hitIndex);
		this->SelectionEnd = CuiTextEdit::GetLineEndIndex(this->buffer, hitIndex);
		if (this->SelectionStart == this->SelectionEnd && this->SelectionEnd < (int)this->buffer.size())
			this->SelectionEnd = GetNextCaretIndex(this->SelectionEnd);
		this->selRangeDirty = true;
		UpdateScroll();
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_KEYDOWN:
	{
		if (!this->ReadOnly && wParam == VK_TAB && this->AllowTabInput)
		{
			this->InputText(L"\t");
			this->selRangeDirty = true;
			UpdateScroll();
			this->InvalidateVisual();
			return true;
		}
		if (!this->ReadOnly && (GetAsyncKeyState(VK_CONTROL) & 0x8000))
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
			auto pos = this->GetAbsoluteLocationDip();
			pos.x += this->selectedPos.x;
			pos.y += this->selectedPos.y;
			float caretH = (this->Font && this->Font->FontHeight > 0.0f) ? this->Font->FontHeight : 16.0f;
			this->ParentForm->SetImeCompositionWindowFromLogicalRect(
				D2D1_RECT_F{ (float)pos.x, (float)pos.y, (float)pos.x + 1.0f, (float)pos.y + caretH });
		}
		if (wParam == VK_DELETE)
		{
			if (!this->ReadOnly)
			{
				this->InputDelete();
				UpdateScroll();
			}
		}
		else if (wParam == VK_RIGHT)
		{
			const bool extendSelection = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->buffer.size());
			if (!extendSelection && span.HasSelection())
			{
				this->SelectionStart = this->SelectionEnd = span.end;
				this->selRangeDirty = true;
				UpdateScroll();
			}
			else if (this->SelectionEnd < (int)this->buffer.size())
			{
				this->SelectionEnd = GetNextCaretIndex(this->SelectionEnd);
				if (!extendSelection)
					this->SelectionStart = this->SelectionEnd;
				this->selRangeDirty = true;
				UpdateScroll();
			}
		}
		else if (wParam == VK_LEFT)
		{
			const bool extendSelection = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			auto span = CuiTextEdit::NormalizeSelection(this->SelectionStart, this->SelectionEnd, this->buffer.size());
			if (!extendSelection && span.HasSelection())
			{
				this->SelectionStart = this->SelectionEnd = span.start;
				this->selRangeDirty = true;
				UpdateScroll();
			}
			else if (this->SelectionEnd > 0)
			{
				this->SelectionEnd = GetPreviousCaretIndex(this->SelectionEnd);
				if (!extendSelection)
					this->SelectionStart = this->SelectionEnd;
				this->selRangeDirty = true;
				UpdateScroll();
			}
		}
		else if (wParam == VK_UP)
		{
			auto font = this->Font;
			if (this->_isVirtualized)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
					this->SelectionEnd = HitTestGlobalIndex(cx, cy - font->FontHeight);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->_textLayoutCache, (UINT32)this->SelectionEnd, (UINT32)0);
				this->SelectionEnd = font->HitTestTextPosition(this->_textLayoutCache, hit[0].left, hit[0].top - (font->FontHeight * 0.5f));
			}
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd < 0)
			{
				this->SelectionEnd = 0;
			}
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (wParam == VK_DOWN)
		{
			auto font = this->Font;
			if (this->_isVirtualized)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
					this->SelectionEnd = HitTestGlobalIndex(cx, cy + font->FontHeight);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->_textLayoutCache, (UINT32)this->SelectionEnd, (UINT32)0);
				this->SelectionEnd = font->HitTestTextPosition(this->_textLayoutCache, hit[0].left, hit[0].top + (font->FontHeight * 1.5f));
			}
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd > (int)this->buffer.size())
			{
				this->SelectionEnd = (int)this->buffer.size();
			}
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (wParam == VK_HOME)
		{
			const bool controlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			this->SelectionEnd = controlDown ? 0 : CuiTextEdit::GetLineStartIndex(this->buffer, this->SelectionEnd);
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->SelectionStart = this->SelectionEnd;
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (wParam == VK_END)
		{
			const bool controlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			this->SelectionEnd = controlDown ? (int)this->buffer.size() : CuiTextEdit::GetLineEndIndex(this->buffer, this->SelectionEnd);
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->SelectionStart = this->SelectionEnd;
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (wParam == VK_PRIOR)
		{
			auto font = this->Font;
			if (this->_isVirtualized)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
					this->SelectionEnd = HitTestGlobalIndex(cx, cy - this->Height);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->_textLayoutCache, (UINT32)this->SelectionEnd, (UINT32)0);
				this->SelectionEnd = font->HitTestTextPosition(this->_textLayoutCache, hit[0].left, hit[0].top - this->Height);
			}
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd < 0)
			{
				this->SelectionEnd = 0;
			}
			this->selRangeDirty = true;
			UpdateScroll(true);
		}
		else if (wParam == VK_NEXT)
		{
			auto font = this->Font;
			if (this->_isVirtualized)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
					this->SelectionEnd = HitTestGlobalIndex(cx, cy + this->Height);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->_textLayoutCache, (UINT32)this->SelectionEnd, (UINT32)0);
				this->SelectionEnd = font->HitTestTextPosition(this->_textLayoutCache, hit[0].left, hit[0].top + this->Height);
			}
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd > (int)this->buffer.size())
			{
				this->SelectionEnd = (int)this->buffer.size();
			}
			this->selRangeDirty = true;
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
			if (!this->ReadOnly)
			{
				const wchar_t c[] = { ch,L'\0' };
				this->InputText(c);
				UpdateScroll(this->SelectionEnd >= (int)this->buffer.size());
			}
		}
		else if (ch == 13 && this->AllowMultiLine)
		{
			if (!this->ReadOnly)
			{
				const wchar_t c[] = { L'\r',L'\n',L'\0' };
				this->InputText(c);
				UpdateScroll(true);
			}
		}
		else if (ch == 1)
		{
			this->SelectionStart = 0;
			this->SelectionEnd = (int)this->buffer.size();
			UpdateScroll();
			this->selRangeDirty = true;
		}
		else if (ch == 8)
		{
			if (!this->ReadOnly && this->buffer.size() > 0)
			{
				this->InputBack();
				UpdateScroll();
			}
		}
		else if (ch == 22)
		{
			std::wstring clipboardText;
			if (!this->ReadOnly && TryReadClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, clipboardText))
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
			if (!this->ReadOnly)
			{
				this->InputBack();
				UpdateScroll();
			}
		}
		this->InvalidateVisual();
	}
	break;
	case WM_IME_COMPOSITION:
	{
		if (this->ReadOnly)
			return true;
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
