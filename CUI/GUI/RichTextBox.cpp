#pragma once
#include "RichTextBox.h"
#include "Form.h"
#include <algorithm>
#pragma comment(lib, "Imm32.lib")
UIClass RichTextBox::Type() { return UIClass::UI_RichTextBox; }

CursorKind RichTextBox::QueryCursor(int xof, int yof)
{
	(void)yof;
	if (!this->Enable) return CursorKind::Arrow;

	const float renderHeight = (float)this->Height - (this->TextMargin * 2.0f);
	const bool hasVScroll = (renderHeight > 0.0f) && (this->textSize.height > renderHeight);
	if (hasVScroll && xof >= (this->Width - 8))
		return CursorKind::SizeNS;

	return CursorKind::IBeam;
}
RichTextBox::RichTextBox(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->buffer = text;
	this->bufferSyncedFromControl = true;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = Colors::LightGray;
	AllowMultiLine = true;
	UpdateLayout();
}

void RichTextBox::SyncBufferFromControlIfNeeded()
{
	if (!this->bufferSyncedFromControl || this->TextChanged)
	{
		this->buffer = this->Text;
		this->bufferSyncedFromControl = true;
	}
}

void RichTextBox::SyncControlTextFromBuffer(const std::wstring& oldText)
{
	this->setTextPrivate(this->buffer);
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
	if (!this->layOutCache)
		return;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int selLen = sele - sels;
	selRange = font->HitTestTextRange(this->layOutCache, (UINT32)sels, (UINT32)selLen);

	this->layOutCache->SetDrawingEffect(NULL, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	this->layOutCache->SetDrawingEffect(d2d->GetBackColorBrush(this->SelectedForeColor), DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen });
	this->selRangeDirty = false;
}
void RichTextBox::UpdateLayout()
{
	if (!this->ParentForm)
		return;
	SyncBufferFromControlIfNeeded();

	this->virtualMode = (this->EnableVirtualization && this->AllowMultiLine && this->buffer.size() >= this->VirtualizeThreshold);
	if (this->virtualMode)
	{
		if (this->layOutCache)
		{
			this->layOutCache->Release();
			this->layOutCache = NULL;
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
		if (this->layOutCache)this->layOutCache->Release();
		auto d2d = this->ParentForm->Render;
		if (d2d)
		{
			auto font = this->Font;
			float render_width = this->Width - (TextMargin * 2.0f);
			float render_height = this->Height - (TextMargin * 2.0f);

			this->layOutCache = d2d->CreateStringLayout(this->buffer, render_width, render_height, font);
			textSize = font->GetTextSize(layOutCache);
			if (textSize.height > render_height)
			{
				if (this->layOutCache) this->layOutCache->Release();
				this->layOutCache = d2d->CreateStringLayout(this->buffer, render_width - 8.0f, render_height, font);
				textSize = font->GetTextSize(layOutCache);
			}
			if (this->layOutCache)
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
			b.layout = NULL;
		}
	}
	this->blocks.clear();
	this->blockTops.clear();
	this->blocksDirty = true;
	this->blockMetricsDirty = true;
	this->virtualTotalHeight = 0.0f;
	this->layoutWidthHasScrollBar = false;
	this->cachedRenderWidth = 0.0f;
}

void RichTextBox::RebuildBlocks()
{
	ReleaseBlocks();
	this->blocksDirty = false;
	this->blockMetricsDirty = true;

	const size_t n = this->buffer.size();
	if (n == 0) return;

	const size_t blockSize = std::max((size_t)256, this->BlockCharCount);
	size_t i = 0;
	while (i < n)
	{
		size_t len = std::min(blockSize, n - i);
		if (i + len < n)
		{
			wchar_t last = this->buffer[i + len - 1];
			wchar_t next = this->buffer[i + len];
			bool lastHigh = (last >= 0xD800 && last <= 0xDBFF);
			bool nextLow = (next >= 0xDC00 && next <= 0xDFFF);
			if (lastHigh && nextLow)
			{
				len += 1;
			}
		}
		TextBlock b;
		b.start = i;
		b.len = len;
		this->blocks.push_back(b);
		i += len;
	}
}

void RichTextBox::EnsureBlockLayout(int idx, float renderWidth, float renderHeight)
{
	if (idx < 0 || idx >= (int)this->blocks.size()) return;
	auto& b = this->blocks[idx];
	if (b.layout && b.height >= 0.0f) return;

	auto d2d = this->ParentForm->Render;
	auto font = this->Font;

	std::wstring s = this->buffer.substr(b.start, b.len);
	b.layout = d2d->CreateStringLayout(s, renderWidth, FLT_MAX, font);
	auto sz = font->GetTextSize(b.layout);
	b.height = sz.height;
	if (b.height < font->FontHeight) b.height = font->FontHeight;
}

void RichTextBox::EnsureAllBlockMetrics(float renderWidth, float renderHeight)
{
	if (!this->blockMetricsDirty && this->cachedRenderWidth == renderWidth)
		return;

	this->cachedRenderWidth = renderWidth;
	this->virtualTotalHeight = 0.0f;
	this->blockTops.resize(this->blocks.size());

	auto compute = [&](float w) {
		for (auto& b : this->blocks)
		{
			if (b.layout)
			{
				b.layout->Release();
				b.layout = NULL;
			}
			b.height = -1.0f;
		}
		float y = 0.0f;
		for (int i = 0; i < (int)this->blocks.size(); i++)
		{
			this->blockTops[i] = y;
			EnsureBlockLayout(i, w, renderHeight);
			y += this->blocks[i].height;
		}
		return y;
		};

	float total = compute(renderWidth);
	bool needScrollBar = total > renderHeight;
	if (needScrollBar)
	{
		total = compute(std::max(0.0f, renderWidth - 8.0f));
		this->layoutWidthHasScrollBar = true;
	}
	else
	{
		this->layoutWidthHasScrollBar = false;
	}
	this->virtualTotalHeight = total;
	this->blockMetricsDirty = false;
}

int RichTextBox::HitTestGlobalIndex(float x, float y)
{
	if (!this->virtualMode || this->blocks.empty()) return 0;
	float renderHeight = this->Height - (TextMargin * 2.0f);
	float renderWidth = this->Width - (TextMargin * 2.0f);
	if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

	float contentY = (y + this->OffsetY) - this->TextMargin;
	if (contentY < 0) contentY = 0;

	int idx = 0;
	for (int i = 0; i < (int)this->blockTops.size(); i++)
	{
		if (contentY >= this->blockTops[i])
			idx = i;
		else
			break;
	}
	EnsureBlockLayout(idx, renderWidth, renderHeight);
	float yInBlock = contentY - this->blockTops[idx];
	float xInBlock = x - this->TextMargin;
	if (xInBlock < 0) xInBlock = 0;

	int local = this->Font->HitTestTextPosition(this->blocks[idx].layout, xInBlock, yInBlock);
	int global = (int)this->blocks[idx].start + local;
	global = std::clamp(global, 0, (int)this->buffer.size());
	return global;
}

bool RichTextBox::GetCaretMetrics(int caretIndex, float& outX, float& outY, float& outH)
{
	outX = outY = outH = 0.0f;
	if (!this->virtualMode || this->blocks.empty()) return false;

	float renderHeight = this->Height - (TextMargin * 2.0f);
	float renderWidth = this->Width - (TextMargin * 2.0f);
	if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

	caretIndex = std::clamp(caretIndex, 0, (int)this->buffer.size());
	int blockIdx = 0;
	for (int i = 0; i < (int)this->blocks.size(); i++)
	{
		if (caretIndex >= (int)this->blocks[i].start && caretIndex <= (int)(this->blocks[i].start + this->blocks[i].len))
		{
			blockIdx = i;
			break;
		}
	}
	EnsureBlockLayout(blockIdx, renderWidth, renderHeight);
	int local = caretIndex - (int)this->blocks[blockIdx].start;
	auto hit = this->Font->HitTestTextRange(this->blocks[blockIdx].layout, (UINT32)local, (UINT32)0);
	if (hit.empty()) return false;
	outX = hit[0].left + this->TextMargin;
	outY = (this->blockTops[blockIdx] + hit[0].top) - this->OffsetY + this->TextMargin;
	outH = hit[0].height;
	return true;
}
void RichTextBox::DrawScroll()
{
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto font = this->Font;
	auto size = this->ActualSize();
	float _render_width = this->Width - (TextMargin * 2.0f);
	float _render_height = this->Height - (TextMargin * 2.0f);
	int max_scroll = textSize.height - _render_height;
	if (this->OffsetY > max_scroll)
	{
		this->OffsetY = max_scroll;
		if (this->OffsetY < 0)this->OffsetY = 0;
	}
	if (textSize.height > _render_height)
	{
		float scroll_block_height = (_render_height / textSize.height) * _render_height;
		if (scroll_block_height < this->Height * 0.1)scroll_block_height = this->Height * 0.1;
		float scroll_block_move_space = this->Height - scroll_block_height;
		float yt = scroll_block_height * 0.5f;
		float yb = this->Height - (scroll_block_height * 0.5f);
		float per = (float)this->OffsetY / (float)max_scroll;
		float scroll_tmp_y = per * scroll_block_move_space;
		float scroll_block_top = scroll_tmp_y;
		d2d->FillRoundRect(abslocation.x + (this->Width - 8.0f), abslocation.y, 8.0f, this->Height, this->ScrollBackColor, 4.0f);
		d2d->FillRoundRect(abslocation.x + (this->Width - 8.0f), abslocation.y + scroll_block_top, 8.0f, scroll_block_height, this->ScrollForeColor, 4.0f);
	}
}

void RichTextBox::ScrollToEnd()
{
	this->UpdateLayout();
	float _render_height = this->Height - (TextMargin * 2.0f);
	int max_scroll = textSize.height - _render_height;
	this->OffsetY = max_scroll;
	if (this->OffsetY < 0)this->OffsetY = 0;
	this->SelectionEnd = this->SelectionStart = (int)this->buffer.size();
	this->PostRender();
}
void RichTextBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;

	float _render_height = this->Height - (TextMargin * 2.0f);
	int maxScroll = textSize.height - _render_height;

	float scrollBlockHeight = (_render_height / textSize.height) * _render_height;
	if (scrollBlockHeight < this->Height * 0.1)scrollBlockHeight = this->Height * 0.1;

	float fontHeight = this->Font->FontHeight;
	int renderItemCount = this->Height / fontHeight;

	float scrollHeight = this->Height - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float grab = std::clamp(_scrollThumbGrabOffsetY, 0.0f, scrollBlockHeight);
	float targetTop = posY - grab;
	float per = targetTop / scrollHeight;
	per = std::clamp(per, 0.0f, 1.0f);
	int newScroll = per * maxScroll;
	{
		this->OffsetY = newScroll;
		if (this->OffsetY < 0) this->OffsetY = 0;
		if (this->OffsetY > maxScroll + 1) this->OffsetY = maxScroll + 1;
		PostRender();
	}
}
void RichTextBox::SetScrollByPos(float yof)
{
	const float renderHeight = this->Height - (TextMargin * 2.0f);
	if (renderHeight <= 0.0f || textSize.height <= 0.0f)
	{
		this->OffsetY = 0.0f;
		return;
	}

	if (textSize.height <= renderHeight)
	{
		this->OffsetY = 0.0f;
		return;
	}

	const float maxScroll = std::max(0.0f, textSize.height - renderHeight);

	float scrollBlockHeight = (renderHeight / textSize.height) * renderHeight;
	if (scrollBlockHeight < this->Height * 0.1f) scrollBlockHeight = this->Height * 0.1f;
	if (scrollBlockHeight > this->Height) scrollBlockHeight = this->Height;

	const float topPosition = scrollBlockHeight * 0.5f;
	const float bottomPosition = this->Height - topPosition;
	if (bottomPosition > topPosition)
	{
		const float percent = std::clamp((yof - topPosition) / (bottomPosition - topPosition), 0.0f, 1.0f);
		this->OffsetY = maxScroll * percent;
	}
	this->OffsetY = std::clamp(this->OffsetY, 0.0f, maxScroll);
}
void RichTextBox::InputText(std::wstring input)
{
	SyncBufferFromControlIfNeeded();
	TrimToMaxLength();
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;

	std::wstring oldText = this->buffer;
	sels = std::clamp(sels, 0, (int)this->buffer.size());
	sele = std::clamp(sele, 0, (int)this->buffer.size());

	if (sels == sele && sels == (int)this->buffer.size())
	{
		this->buffer.append(input);
		SelectionEnd = SelectionStart = (int)this->buffer.size();
	}
	else
	{
		if (sele > sels)
			this->buffer.erase((size_t)sels, (size_t)(sele - sels));
		this->buffer.insert((size_t)sels, input);
		SelectionEnd = SelectionStart = sels + (int)input.size();
	}

	if (!this->AllowMultiLine)
	{
		for (auto& ch : this->buffer)
		{
			if (ch == L'\r' || ch == L'\n')
			{
				ch = L' ';
			}
		}
	}
	TrimToMaxLength();
	this->selRangeDirty = true;
	this->blocksDirty = true;
	SyncControlTextFromBuffer(oldText);
}
void RichTextBox::InputBack()
{
	SyncBufferFromControlIfNeeded();
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int selLen = sele - sels;
	std::wstring oldText = this->buffer;
	sels = std::clamp(sels, 0, (int)this->buffer.size());
	sele = std::clamp(sele, 0, (int)this->buffer.size());
	selLen = sele - sels;

	if (selLen > 0)
	{
		this->SelectionStart = this->SelectionEnd = sels;
		this->buffer.erase((size_t)sels, (size_t)selLen);
	}
	else if (sels > 0)
	{
		this->buffer.erase((size_t)sels - 1, 1);
		this->SelectionStart = this->SelectionEnd = sels - 1;
	}
	this->selRangeDirty = true;
	this->blocksDirty = true;
	SyncControlTextFromBuffer(oldText);
}
void RichTextBox::InputDelete()
{
	SyncBufferFromControlIfNeeded();
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int selLen = sele - sels;
	std::wstring oldText = this->buffer;
	sels = std::clamp(sels, 0, (int)this->buffer.size());
	sele = std::clamp(sele, 0, (int)this->buffer.size());
	selLen = sele - sels;

	if (selLen > 0)
	{
		this->SelectionStart = this->SelectionEnd = sels;
		this->buffer.erase((size_t)sels, (size_t)selLen);
	}
	else if (sels < (int)this->buffer.size())
	{
		this->buffer.erase((size_t)sels, 1);
		this->SelectionStart = this->SelectionEnd = sels;
	}
	this->selRangeDirty = true;
	this->blocksDirty = true;
	SyncControlTextFromBuffer(oldText);
}
void RichTextBox::UpdateScroll(bool arrival)
{
	if (this->TextChanged || (this->virtualMode && (this->blocksDirty || this->blockMetricsDirty)) || (!this->virtualMode && this->layOutCache == NULL))
	{
		this->UpdateLayout();
	}

	if (this->virtualMode)
	{
		float cx, cy, ch;
		if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
		{
			float render_height = this->Height - (TextMargin * 2.0f);
			float caretTopContent = (cy - this->TextMargin) + this->OffsetY;
			float caretBottomContent = caretTopContent + ch;
			if (arrival && this->SelectionEnd >= (int)this->buffer.size())
			{
				const float maxScroll = std::max(0.0f, this->textSize.height - render_height);
				this->OffsetY = maxScroll;
			}
			else if (caretBottomContent - this->OffsetY > render_height)
			{
				this->OffsetY = caretBottomContent - render_height;
			}
			if (caretTopContent - this->OffsetY < 0.0f)
				this->OffsetY = caretTopContent;
			if (this->OffsetY < 0) this->OffsetY = 0;
		}
		return;
	}
	float render_width = this->Width - (TextMargin * 2.0f);
	float render_height = this->Height - (TextMargin * 2.0f);
	if (textSize.height > render_height)
		render_width -= 8.0f;
	auto font = this->Font;
	auto selected = font->HitTestTextRange(this->layOutCache, (UINT32)SelectionEnd, (UINT32)0);
	if (selected.size() > 0)
	{
		auto lastSelect = selected[0];
		if (arrival && this->SelectionEnd >= (int)this->buffer.size())
		{
			const float maxScroll = std::max(0.0f, this->textSize.height - render_height);
			OffsetY = maxScroll;
		}
		else if ((lastSelect.top + lastSelect.height) - OffsetY > render_height)
		{
			OffsetY = (lastSelect.top + lastSelect.height) - render_height;
		}
		if (lastSelect.top - OffsetY < 0.0f)
		{
			OffsetY = lastSelect.top;
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
	this->InputText(str + L"\r");
	this->selRangeDirty = true;
}
std::wstring RichTextBox::GetSelectedString()
{
	SyncBufferFromControlIfNeeded();
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	if (sele > sels)
	{
		sels = std::clamp(sels, 0, (int)this->buffer.size());
		sele = std::clamp(sele, 0, (int)this->buffer.size());
		return this->buffer.substr((size_t)sels, (size_t)(sele - sels));
	}
	return L"";
}
void RichTextBox::Update()
{
	if (this->IsVisual == false)return;
	this->UpdateLayout();
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	bool isSelected = this->ParentForm->Selected == this;
	this->_caretRectCacheValid = false;

	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, isSelected ? this->FocusedColor : this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		if (this->buffer.size() > 0)
		{
			auto font = this->Font;
			if (this->virtualMode)
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
						const float ax = (float)abslocation.x + cx;
						const float ay = (float)abslocation.y + cy;
						const float ah = (ch > 0.0f) ? ch : font->FontHeight;
						this->_caretRectCache = { ax - 2.0f, ay - 2.0f, ax + 2.0f, ay + ah + 2.0f };
						this->_caretRectCacheValid = true;
					}
					d2d->DrawLine(
						{ (float)abslocation.x + cx, (float)abslocation.y + cy },
						{ (float)abslocation.x + cx, (float)abslocation.y + cy + ch },
						Colors::Black);
				}

				float viewTop = this->OffsetY;
				float viewBottom = this->OffsetY + renderHeight;

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
					float drawY = ((float)abslocation.y + TextMargin) + (top - this->OffsetY);
					float drawX = (float)abslocation.x + TextMargin;

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
							sr.left + abslocation.x + TextMargin,
							(sr.top + abslocation.y + TextMargin) - this->OffsetY,
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
						const float ax = caret.left + (float)abslocation.x + TextMargin;
						const float ay = (caret.top + (float)abslocation.y + TextMargin) - this->OffsetY;
						const float ah = caret.height > 0 ? caret.height : font->FontHeight;
						this->_caretRectCache = { ax - 2.0f, ay - 2.0f, ax + 2.0f, ay + ah + 2.0f };
						this->_caretRectCacheValid = true;
					}
					if (!selRange.empty())
						d2d->DrawLine(
							{ selRange[0].left + abslocation.x + TextMargin,(selRange[0].top + abslocation.y + TextMargin) - this->OffsetY },
							{ selRange[0].left + abslocation.x + TextMargin,(selRange[0].top + abslocation.y + selRange[0].height + TextMargin) - this->OffsetY },
							Colors::Black);
				}
				if (!selRange.empty())
				{
					selectedPos = { (int)selRange[0].left , (int)selRange[0].top };
					selectedPos.y -= this->OffsetY;
					selectedPos.y += this->TextMargin;
					selectedPos.x += this->TextMargin;
				}
				d2d->DrawStringLayout(this->layOutCache,
					(float)abslocation.x + TextMargin, ((float)abslocation.y + TextMargin) - this->OffsetY,
					this->ForeColor);
			}
			else
			{
				d2d->DrawStringLayout(this->layOutCache,
					(float)abslocation.x + TextMargin, ((float)abslocation.y + TextMargin) - this->OffsetY,
					this->ForeColor);
			}
		}
		else
		{
			if (isSelected)
			{
				const float ax = (float)TextMargin + (float)abslocation.x;
				const float ay = (float)abslocation.y;
				const float ah = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				this->_caretRectCache = { ax - 2.0f, ay - 2.0f, ax + 2.0f, ay + ah + 2.0f };
				this->_caretRectCacheValid = true;
				d2d->DrawLine(
					{ (float)TextMargin + (float)abslocation.x , (float)abslocation.y },
					{ (float)TextMargin + (float)abslocation.x , (float)abslocation.y + 16.0f },
					Colors::Black);
			}
		}
		this->DrawScroll();
		d2d->DrawRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
}

bool RichTextBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!this->IsSelected()) return false;
	if (this->SelectionStart != this->SelectionEnd) return false;
	if (!this->_caretRectCacheValid) return false;
	outRect = this->_caretRectCache;
	return true;
}
bool RichTextBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		List<std::wstring> files;
		for (int i = 0; i < uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.Add(strFileName);
		}
		DragFinish(hDropInfo);
		if (files.Count > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
		{
			if (this->OffsetY > 0)
			{
				this->OffsetY -= 10;
				if (this->OffsetY < 0)this->OffsetY = 0;
				this->PostRender();
			}
		}
		else
		{
			auto font = this->Font;
			float render_width = this->Width - (TextMargin * 2.0f);
			float render_height = this->Height - (TextMargin * 2.0f);
			if (textSize.height > render_height)render_width -= 8.0f;
			if (this->OffsetY < textSize.height - render_height)
			{
				this->OffsetY += 10;
				if (this->OffsetY > textSize.height - render_height)this->OffsetY = textSize.height - render_height;
				this->PostRender();
			}
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, event_obj);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (isDraggingScroll) {
			UpdateScrollDrag(yof);
		}
		if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && this->ParentForm->Selected == this && !isDraggingScroll)
		{
			auto font = this->Font;
			if (this->virtualMode)
				SelectionEnd = HitTestGlobalIndex((float)xof, (float)yof);
			else
				SelectionEnd = font->HitTestTextPosition(this->layOutCache, xof - TextMargin, (yof + this->OffsetY) - TextMargin);
			UpdateScroll();
			this->PostRender();
			this->selRangeDirty = true;
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
			if (xof >= Width - 8 && xof <= Width)
			{
				// 竖向滚动条：点在滑块上则用按下点锚定；否则用滑块中心（原行为）
				const float renderHeight = this->Height - (TextMargin * 2.0f);
				if (renderHeight > 0.0f && textSize.height > renderHeight)
				{
					const float maxScroll = std::max(0.0f, textSize.height - renderHeight);
					float thumbH = (renderHeight / textSize.height) * renderHeight;
					if (thumbH < this->Height * 0.1f) thumbH = this->Height * 0.1f;
					if (thumbH > this->Height) thumbH = this->Height;
					const float moveSpace = std::max(0.0f, (float)this->Height - thumbH);
					float per = 0.0f;
					if (maxScroll > 0.0f) per = std::clamp(this->OffsetY / maxScroll, 0.0f, 1.0f);
					const float thumbTop = per * moveSpace;
					const float localY = (float)yof;
					const bool hitThumb = (localY >= thumbTop && localY <= (thumbTop + thumbH));
					_scrollThumbGrabOffsetY = hitThumb ? (localY - thumbTop) : (thumbH * 0.5f);
				}
				else
				{
					_scrollThumbGrabOffsetY = 0.0f;
				}
				isDraggingScroll = true;
				UpdateScrollDrag((float)yof);
				this->PostRender();
			}
			else
			{
				auto font = this->Font;
				if (this->virtualMode)
					this->SelectionStart = this->SelectionEnd = HitTestGlobalIndex((float)xof, (float)yof);
				else
					this->SelectionStart = this->SelectionEnd = font->HitTestTextPosition(this->layOutCache, xof - TextMargin, (yof + this->OffsetY) - TextMargin);
				this->selRangeDirty = true;
			}
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
		if (isDraggingScroll) {
			isDraggingScroll = false;
		}
		else if (this->ParentForm->Selected == this)
		{
			auto font = this->Font;
			if (this->virtualMode)
				SelectionEnd = HitTestGlobalIndex((float)xof, (float)yof);
			else
				SelectionEnd = font->HitTestTextPosition(this->layOutCache, xof - TextMargin, (yof + this->OffsetY) - TextMargin);
			this->selRangeDirty = true;
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
		if (wParam == VK_TAB && this->AllowTabInput)
		{
			this->InputText(L"\t");
			this->selRangeDirty = true;
			UpdateScroll();
			this->PostRender();
			return true;
		}

		auto pos = this->AbsLocation;
		pos.x += this->selectedPos.x;
		pos.y += this->selectedPos.y;
		HIMC hImc = ImmGetContext(this->ParentForm->Handle);
		COMPOSITIONFORM form;
		form.dwStyle = CFS_RECT;
		form.ptCurrentPos = pos;
		form.rcArea = RECT{ pos.x, pos.y + this->Height, pos.x + 300, pos.y + 240 };
		ImmSetCompositionWindow(hImc, &form);
		if (wParam == VK_DELETE)
		{
			this->InputDelete();
			UpdateScroll();
		}
		else if (wParam == VK_RIGHT)
		{
			if (this->SelectionEnd < this->Text.size())
			{
				this->SelectionEnd = this->SelectionEnd + 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				{
					this->SelectionStart = this->SelectionEnd;
				}
				if (this->SelectionEnd > this->Text.size())
				{
					this->SelectionEnd = this->Text.size();
				}
				this->selRangeDirty = true;
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
				this->selRangeDirty = true;
				UpdateScroll();
			}
		}
		else if (wParam == VK_UP)
		{
			auto font = this->Font;
			if (this->virtualMode)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
					this->SelectionEnd = HitTestGlobalIndex(cx, cy - font->FontHeight);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->layOutCache, (UINT32)this->SelectionEnd, (UINT32)0);
				this->SelectionEnd = font->HitTestTextPosition(this->layOutCache, hit[0].left, hit[0].top - (font->FontHeight * 0.5f));
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
			if (this->virtualMode)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
					this->SelectionEnd = HitTestGlobalIndex(cx, cy + font->FontHeight);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->layOutCache, (UINT32)this->SelectionEnd, (UINT32)0);
				this->SelectionEnd = font->HitTestTextPosition(this->layOutCache, hit[0].left, hit[0].top + (font->FontHeight * 1.5f));
			}
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd > this->Text.size())
			{
				this->SelectionEnd = this->Text.size();
			}
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (wParam == VK_HOME)
		{
			this->SelectionEnd = 0;
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
		else if (wParam == VK_END)
		{
			this->SelectionEnd = this->Text.size();
			if (this->SelectionEnd > this->Text.size())
			{
				this->SelectionEnd = this->Text.size();
			}
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (wParam == VK_PRIOR)
		{
			auto font = this->Font;
			if (this->virtualMode)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
					this->SelectionEnd = HitTestGlobalIndex(cx, cy - this->Height);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->layOutCache, (UINT32)this->SelectionEnd, (UINT32)0);
				this->SelectionEnd = font->HitTestTextPosition(this->layOutCache, hit[0].left, hit[0].top - this->Height);
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
			if (this->virtualMode)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->SelectionEnd, cx, cy, ch))
					this->SelectionEnd = HitTestGlobalIndex(cx, cy + this->Height);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->layOutCache, (UINT32)this->SelectionEnd, (UINT32)0);
				this->SelectionEnd = font->HitTestTextPosition(this->layOutCache, hit[0].left, hit[0].top + this->Height);
			}
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd > this->Text.size())
			{
				this->SelectionEnd = this->Text.size();
			}
			this->selRangeDirty = true;
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
			UpdateScroll(this->SelectionEnd >= (int)this->buffer.size());
		}
		else if (ch == 13 && this->AllowMultiLine)
		{
			const wchar_t c[] = { L'\n',L'\0' };
			this->InputText(c);
			UpdateScroll(true);
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
			if (this->buffer.size() > 0)
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
			HIMC hIMC = ImmGetContext(this->ParentForm->Handle);
			DWORD bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
			if (bytes > 0)
			{
				std::wstring buffer;
				buffer.resize(bytes / sizeof(wchar_t));
				ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buffer.data(), bytes);
				std::wstring filtered;
				filtered.reserve(buffer.size());
				for (wchar_t c : buffer)
				{
					if (c > 255) filtered.push_back(c);
				}
				if (!filtered.empty())
				{
					this->InputText(filtered);
				}
			}
			ImmReleaseContext(this->ParentForm->Handle, hIMC);
			UpdateScroll();
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
