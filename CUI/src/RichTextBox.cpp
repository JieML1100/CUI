#pragma once
#define NOMINMAX
#include "RichTextBox.h"
#include "Window.h"
#include "TextEditCore.h"
#include <algorithm>
#include <cstring>

namespace
{
	constexpr bool RichTextIsMultiLine = true;

	CuiTextEdit::EditOptions RichEditOptions()
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = RichTextIsMultiLine;
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

const DependencyProperty& RichTextBox::TextProperty()
{
	RegisterDependencyProperties();
	const std::type_index ownerTypes[] = {
		std::type_index(typeid(RichTextBox))
	};
	const auto* metadata =
		DependencyPropertyRegistry::FindRegistered(ownerTypes, L"Text");
	if (!metadata)
		throw std::logic_error(
			"RichTextBox compatibility Text property is not registered");
	return metadata->Property();
}

GET_CPP(RichTextBox, std::wstring, Text) { return Control::GetText(); }
SET_CPP(RichTextBox, std::wstring, Text)
{
	Control::SetText(std::move(value));
}

GET_CPP(RichTextBox, bool, AcceptsTab) { return _acceptsTab; }
SET_CPP(RichTextBox, bool, AcceptsTab)
{
	(void)SetPropertyField(L"AcceptsTab", _acceptsTab, value);
}

GET_CPP(RichTextBox, bool, IsReadOnly) { return _isReadOnly; }
SET_CPP(RichTextBox, bool, IsReadOnly)
{
	(void)SetPropertyField(L"IsReadOnly", _isReadOnly, value);
}

GET_CPP(RichTextBox, int, MaxLength) { return _maxLength; }
SET_CPP(RichTextBox, int, MaxLength)
{
	if (!SetPropertyField(L"MaxLength", _maxLength, value)) return;
	SyncBufferFromControlIfNeeded();
	TrimToMaxLength();
	_textLayoutDirty = true;
	RequestLayout();
	InvalidateVisual();
}

void RichTextBox::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<RichTextBox, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		options.Changed = [](RichTextBox& target,
			const std::wstring& oldValue, const std::wstring& newValue)
		{
			target.bufferSyncedFromControl = false;
			target._textLayoutDirty = true;
			TextChangedEventArgs args(oldValue, newValue);
			target.OnTextChanged(&target, args);
		};
		DependencyPropertyRegistry::Register<RichTextBox, std::wstring>(
			L"Text",
			[](RichTextBox& target, Handler handler, DataSourceUpdateMode mode)
			{
				if (mode == DataSourceUpdateMode::OnValidation)
					return target.OnLostFocus.Subscribe(
						[handler = std::move(handler)](Control*) { handler(); });
				return target.OnTextChanged.Subscribe(
					[handler = std::move(handler)](
						Control*, TextChangedEventArgs&) { handler(); });
			}, std::move(options));
		auto boolOptions = DependencyPropertyOptions<RichTextBox, bool>{
			false, DependencyPropertyFlags::AffectsRender };
		boolOptions.Design.Category = L"Behavior";
		boolOptions.Design.CategoryOrder = 300;
		boolOptions.Design.Order = 10;
		boolOptions.Design.Editor = DependencyPropertyEditorKind::Boolean;
		boolOptions.Design.Persistence = DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<RichTextBox, bool>(L"AcceptsTab",
			[](RichTextBox& target) { return target.AcceptsTab; },
			[](RichTextBox& target, const bool& value)
			{ target.AcceptsTab = value; }, {}, boolOptions);
		boolOptions.Design.Order = 20;
		DependencyPropertyRegistry::Register<RichTextBox, bool>(L"IsReadOnly",
			[](RichTextBox& target) { return target.IsReadOnly; },
			[](RichTextBox& target, const bool& value)
			{ target.IsReadOnly = value; }, {}, std::move(boolOptions));
		auto maxLengthOptions = DependencyPropertyOptions<RichTextBox, int>{
			0, DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender,
			[](RichTextBox&, const int& proposed) -> std::optional<int>
			{ return (std::max)(0, proposed); } };
		maxLengthOptions.Design.Category = L"Behavior";
		maxLengthOptions.Design.CategoryOrder = 300;
		maxLengthOptions.Design.Order = 30;
		maxLengthOptions.Design.Editor = DependencyPropertyEditorKind::Number;
		maxLengthOptions.Design.Minimum = 0.0;
		maxLengthOptions.Design.Step = 1.0;
		maxLengthOptions.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<RichTextBox, int>(L"MaxLength",
			[](RichTextBox& target) { return target.MaxLength; },
			[](RichTextBox& target, const int& value)
			{ target.MaxLength = value; }, {}, std::move(maxLengthOptions));
		RegisterControlBorderThicknessMetadata<RichTextBox>(1.0f, 60);
		return true;
	}();
	(void)registered;
}

bool RichTextBox::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	UpdateLayout();
	const float renderHeight = TextViewportHeight();
	const float maxScroll = std::max(0.0f, _textSize.height - renderHeight);
	if (renderHeight <= 0.0f || maxScroll <= 0.0f)
		return false;
	if (this->_verticalScrollOffset < 0.0f) this->_verticalScrollOffset = 0.0f;
	if (this->_verticalScrollOffset > maxScroll) this->_verticalScrollOffset = maxScroll;
	return delta > 0
		? this->_verticalScrollOffset > 0.0f
		: this->_verticalScrollOffset < maxScroll;
}

bool RichTextBox::HandlesNavigationKey(Key key) const
{
	if (key == Key::Tab)
		return this->_acceptsTab;
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

CursorKind RichTextBox::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->IsEnabled) return CursorKind::Arrow;

	const float renderHeight = TextViewportHeight();
	const bool hasVScroll = (renderHeight > 0.0f) && (this->_textSize.height > renderHeight);
	if (hasVScroll && localX >= (this->ActualWidth - 8))
		return CursorKind::SizeNS;

	return CursorKind::IBeam;
}
RichTextBox::RichTextBox()
{
	RegisterDependencyProperties();
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness{ 5.0f }),
		DependencyPropertyValueSource::Theme);
	this->buffer = this->Text;
	this->bufferSyncedFromControl = true;
	this->RendererBackgroundColor = cui::theme::palette::Surface;
	this->RendererBorderColor = cui::theme::palette::BorderStrong;
	this->RendererForegroundColor = cui::theme::palette::TextPrimary;
	UpdateLayout();
}

RichTextBox::~RichTextBox()
{
	ReleaseTextLayout();
	ReleaseBlocks();
}

void RichTextBox::NotifySelectionChanged()
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

void RichTextBox::SyncBufferFromControlIfNeeded()
{
	if (!this->bufferSyncedFromControl)
	{
		this->buffer = NormalizeLineBreaks(this->Text);
		this->bufferSyncedFromControl = true;
	}
}

std::wstring RichTextBox::NormalizeLineBreaks(const std::wstring& text) const
{
	return CuiTextEdit::NormalizeInput(text, RichEditOptions());
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
	return CuiTextEdit::GetNextCaretIndex(this->buffer, index, RichTextIsMultiLine);
}

int RichTextBox::GetPreviousCaretIndex(int index) const
{
	return CuiTextEdit::GetPreviousCaretIndex(this->buffer, index, RichTextIsMultiLine);
}

void RichTextBox::NormalizeSelectionRangeForErase(int& start, int& end) const
{
	CuiTextEdit::NormalizeSelectionForTextElements(this->buffer, start, end, RichTextIsMultiLine);
}

bool RichTextBox::GetBackspaceEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const
{
	return CuiTextEdit::GetBackspaceEraseRange(this->buffer, caretIndex, RichTextIsMultiLine, eraseStart, eraseLength);
}

bool RichTextBox::GetDeleteEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const
{
	return CuiTextEdit::GetDeleteEraseRange(this->buffer, caretIndex, RichTextIsMultiLine, eraseStart, eraseLength);
}

void RichTextBox::SyncControlTextFromBuffer(const std::wstring& oldText)
{
	if (oldText == this->buffer)
		return;
	this->highlightRanges.clear();
	this->textStyleRanges.clear();
	// Editing updates the current target value while retaining any Local
	// expression. RichTextBox.Text is a temporary CUI compatibility surface.
	(void)TrySetCurrentPropertyValue(
		L"Text", BindingValue(this->buffer));
	this->bufferSyncedFromControl = true;
}

void RichTextBox::TrimToMaxLength()
{
	if (_maxLength == 0) return;
	if (this->buffer.size() <= static_cast<size_t>(_maxLength)) return;

	const size_t removeCount = this->buffer.size()
		- static_cast<size_t>(_maxLength);
	if (removeCount == 0) return;

	this->buffer = this->buffer.substr(removeCount);

	this->_selectionStart = std::max(0, this->_selectionStart - (int)removeCount);
	this->_selectionEnd = std::max(0, this->_selectionEnd - (int)removeCount);
	if (this->_selectionStart > (int)this->buffer.size()) this->_selectionStart = (int)this->buffer.size();
	if (this->_selectionEnd > (int)this->buffer.size()) this->_selectionEnd = (int)this->buffer.size();
}

void RichTextBox::UpdateSelRange()
{
	if (!this->_textLayoutCache)
		return;
	auto font = this->GetRenderFont();
	int sels = _selectionStart <= _selectionEnd ? _selectionStart : _selectionEnd;
	int sele = _selectionEnd >= _selectionStart ? _selectionEnd : _selectionStart;
	int selLen = sele - sels;
	selRange = font->HitTestTextRange(this->_textLayoutCache, (UINT32)sels, (UINT32)selLen);
	this->selRangeDirty = false;
}

void RichTextBox::ApplyTextDrawingEffects(
	IDWriteTextLayout* layout,
	int textStart,
	int textLength,
	bool includeSelection)
{
	if (!layout || !GetPresentationWindow() || !GetDrawingContext()) return;
	layout->SetDrawingEffect(nullptr, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	const int textEnd = textStart + (std::max)(0, textLength);
	for (const auto& style : textStyleRanges)
	{
		const int rangeStart = (std::max)(style.Start, textStart);
		const int rangeEnd = (std::min)(
			style.Start + style.Length, textEnd);
		if (rangeEnd <= rangeStart) continue;
		auto brush = GetTextStyleBrush(style.ForegroundColor);
		if (!brush) continue;
		layout->SetDrawingEffect(
			brush,
			DWRITE_TEXT_RANGE{
				static_cast<UINT32>(rangeStart - textStart),
				static_cast<UINT32>(rangeEnd - rangeStart) });
	}
	if (!includeSelection) return;
	const int selectionStart = (std::max)(
		(std::min)(_selectionStart, _selectionEnd), textStart);
	const int selectionEnd = (std::min)(
		(std::max)(_selectionStart, _selectionEnd), textEnd);
	if (selectionEnd <= selectionStart) return;
	auto selectionBrush = GetTextStyleBrush(_selectionForeColor);
	if (!selectionBrush) return;
	layout->SetDrawingEffect(
		selectionBrush,
		DWRITE_TEXT_RANGE{
			static_cast<UINT32>(selectionStart - textStart),
			static_cast<UINT32>(selectionEnd - selectionStart) });
}

ID2D1SolidColorBrush* RichTextBox::GetTextStyleBrush(D2D1_COLOR_F color)
{
	if (!GetPresentationWindow() || !GetDrawingContext()) return nullptr;
	auto context = GetDrawingContext()->GetDeviceContextRaw();
	if (!context) return nullptr;
	if (context != textStyleBrushDeviceContext.Get())
	{
		textStyleBrushes.clear();
		textStyleBrushDeviceContext = context;
	}
	for (auto& entry : textStyleBrushes)
	{
		if (entry.Color.r == color.r && entry.Color.g == color.g
			&& entry.Color.b == color.b && entry.Color.a == color.a)
			return entry.Brush.Get();
	}
	TextStyleBrush entry;
	entry.Color = color;
	if (FAILED(context->CreateSolidColorBrush(
		color, entry.Brush.ReleaseAndGetAddressOf())))
		return nullptr;
	textStyleBrushes.push_back(std::move(entry));
	return textStyleBrushes.back().Brush.Get();
}

void RichTextBox::NotifyDeviceResourcesInvalidated() noexcept
{
	textStyleBrushes.clear();
	textStyleBrushDeviceContext.Reset();
	Control::NotifyDeviceResourcesInvalidated();
}

void RichTextBox::UpdateLayout()
{
	auto font = this->GetRenderFont();
	if (font != this->_lastLayoutFont)
	{
		this->_lastLayoutFont = font;
		this->_textLayoutDirty = true;
		this->selRangeDirty = true;
		this->blocksDirty = true;
		this->blockMetricsDirty = true;
		this->_caretRectCacheValid = false;
		ReleaseTextLayout();
		ReleaseBlocks();
	}

	if (!this->GetPresentationWindow())
		return;
	SyncBufferFromControlIfNeeded();

	this->_isVirtualized = (_enableVirtualization
		&& this->buffer.size() >= _virtualizeThreshold);
	if (this->_isVirtualized)
	{
		ReleaseTextLayout();

		float renderWidth = TextViewportWidth();
		float renderHeight = TextViewportHeight();

		if (this->_textLayoutDirty || this->lastLayoutSize.width != this->ActualWidth || this->lastLayoutSize.height != this->ActualHeight || this->blocksDirty)
		{
			RebuildBlocks();
			this->lastLayoutSize = { this->ActualWidth, this->ActualHeight };
			this->_textLayoutDirty = false;
		}

		EnsureAllBlockMetrics(renderWidth, renderHeight);
		this->_textSize.height = this->virtualTotalHeight;
		this->_textSize.width = renderWidth;
		this->selRangeDirty = true;
		return;
	}

	ReleaseBlocks();

	if ((this->_textLayoutDirty || this->lastLayoutSize.width != this->ActualWidth || this->lastLayoutSize.height != this->ActualHeight) && this->GetPresentationWindow())
	{
		// Text formatting is retained model state, not a render-target resource.
		// Input, caret and IME transactions may update it outside an active frame;
		// DrawingContext remains frame-only.
		ReleaseTextLayout();
		auto font = this->GetRenderFont();
		if (font && font->FontObject)
		{
			const float renderWidth =
				(std::max)(1.0f, TextViewportWidth());
			const float renderHeight =
				(std::max)(1.0f, TextViewportHeight());

			this->_textLayoutCache = Factory::CreateStringLayout(
				this->buffer, renderWidth, renderHeight, font->FontObject);
			ApplyRichTextWrapping(this->_textLayoutCache);
			_textSize = font->GetTextSize(_textLayoutCache);
			if (_textSize.height > renderHeight)
			{
				ReleaseTextLayout();
				this->_textLayoutCache = Factory::CreateStringLayout(
					this->buffer, (std::max)(1.0f, renderWidth - 8.0f),
					renderHeight, font->FontObject);
				ApplyRichTextWrapping(this->_textLayoutCache);
				_textSize = font->GetTextSize(_textLayoutCache);
			}
			if (this->_textLayoutCache)
			{
				_textLayoutDirty = false;
				this->lastLayoutSize = { this->ActualWidth, this->ActualHeight };
				this->selRangeDirty = true;
			}
		}
	}
}

void RichTextBox::ReleaseTextLayout() noexcept
{
	if (!this->_textLayoutCache) return;
	this->_textLayoutCache->Release();
	this->_textLayoutCache = nullptr;
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

	const size_t blockSize = (std::max)((size_t)256, _blockCharCount);
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

	auto font = this->GetRenderFont();
	if (!font || !font->FontObject)
	{
		block.height = 0.0f;
		return;
	}

	std::wstring blockText = this->buffer.substr(block.start, block.len);
	block.layout = Factory::CreateStringLayout(
		std::move(blockText), (std::max)(1.0f, renderWidth),
		FLT_MAX, font->FontObject);
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
	float renderHeight = TextViewportHeight();
	float renderWidth = TextViewportWidth();
	if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

	float contentY = (y + this->_verticalScrollOffset) - Padding.Top;
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
	float xInBlock = x - Padding.Left;
	if (xInBlock < 0) xInBlock = 0;

	int localIndex = this->GetRenderFont()->HitTestTextPosition(this->blocks[blockIndex].layout, xInBlock, yInBlock);
	int globalIndex = (int)this->blocks[blockIndex].start + localIndex;
	globalIndex = std::clamp(globalIndex, 0, (int)this->buffer.size());
	return globalIndex;
}

bool RichTextBox::GetCaretMetrics(int caretIndex, float& outX, float& outY, float& outH)
{
	outX = outY = outH = 0.0f;
	if (!this->_isVirtualized || this->blocks.empty()) return false;

	float renderHeight = TextViewportHeight();
	float renderWidth = TextViewportWidth();
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
	auto hit = this->GetRenderFont()->HitTestTextRange(this->blocks[blockIndex].layout, (UINT32)localIndex, (UINT32)0);
	if (hit.empty()) return false;
	outX = hit[0].left + Padding.Left;
	outY = (this->blockTops[blockIndex] + hit[0].top)
		- this->_verticalScrollOffset + Padding.Top;
	outH = hit[0].height;
	return true;
}
void RichTextBox::DrawScroll()
{
	auto d2d = this->GetDrawingContext();
	float renderHeight = TextViewportHeight();
	float maxScroll = _textSize.height - renderHeight;
	if (this->_verticalScrollOffset > maxScroll)
	{
		this->_verticalScrollOffset = maxScroll;
		if (this->_verticalScrollOffset < 0)this->_verticalScrollOffset = 0;
	}
	if (_textSize.height > renderHeight)
	{
		float scrollThumbHeight = (renderHeight / _textSize.height) * renderHeight;
		if (scrollThumbHeight < this->ActualHeight * 0.1f)scrollThumbHeight = this->ActualHeight * 0.1f;
		float scrollThumbMoveSpace = this->ActualHeight - scrollThumbHeight;
		float scrollRatio = (float)this->_verticalScrollOffset / (float)maxScroll;
		float scrollThumbTop = scrollRatio * scrollThumbMoveSpace;
		// 局部坐标：滚动条 X = Width - 8，Y = 0
		d2d->FillRoundRect(this->ActualWidth - 8.0f, 0, 8.0f,
			static_cast<float>(this->ActualHeight), _scrollBackColor, 4.0f);
		d2d->FillRoundRect(this->ActualWidth - 8.0f, scrollThumbTop,
			8.0f, scrollThumbHeight, _scrollForeColor, 4.0f);
	}
}

void RichTextBox::ScrollToEnd()
{
	this->UpdateLayout();
	float renderHeight = TextViewportHeight();
	float maxScroll = _textSize.height - renderHeight;
	this->_verticalScrollOffset = maxScroll;
	if (this->_verticalScrollOffset < 0)this->_verticalScrollOffset = 0;
	this->_selectionEnd = this->_selectionStart = (int)this->buffer.size();
	NotifySelectionChanged();
	this->InvalidateVisual();
}

void RichTextBox::ScrollSelectionIntoView()
{
	SyncBufferFromControlIfNeeded();
	const int textLength = static_cast<int>(buffer.size());
	_selectionStart = (std::clamp)(_selectionStart, 0, textLength);
	_selectionEnd = (std::clamp)(_selectionEnd, 0, textLength);
	NotifySelectionChanged();
	UpdateScroll(_selectionEnd >= textLength);
	selRangeDirty = true;
	InvalidateVisual();
}

void RichTextBox::SetHighlightRanges(
	std::vector<RichTextBoxTextRange> ranges)
{
	SyncBufferFromControlIfNeeded();
	std::vector<RichTextBoxTextRange> normalized;
	normalized.reserve(ranges.size());
	const int textLength = static_cast<int>(buffer.size());
	for (auto range : ranges)
	{
		range.Start = (std::clamp)(range.Start, 0, textLength);
		range.Length = (std::clamp)(range.Length, 0,
			textLength - range.Start);
		if (range.Length > 0) normalized.push_back(range);
	}
	highlightRanges = std::move(normalized);
	InvalidateVisual();
}

void RichTextBox::ClearHighlightRanges()
{
	if (highlightRanges.empty()) return;
	highlightRanges.clear();
	InvalidateVisual();
}

void RichTextBox::SetTextStyleRanges(
	std::vector<RichTextBoxTextStyleRange> ranges)
{
	SyncBufferFromControlIfNeeded();
	std::vector<RichTextBoxTextStyleRange> normalized;
	normalized.reserve(ranges.size());
	const int textLength = static_cast<int>(buffer.size());
	for (auto range : ranges)
	{
		range.Start = (std::clamp)(range.Start, 0, textLength);
		range.Length = (std::clamp)(range.Length, 0,
			textLength - range.Start);
		if (range.Length > 0) normalized.push_back(range);
	}
	textStyleRanges = std::move(normalized);
	InvalidateVisual();
}

void RichTextBox::ClearTextStyleRanges()
{
	if (textStyleRanges.empty()) return;
	textStyleRanges.clear();
	InvalidateVisual();
}

bool RichTextBox::TryGetTextInputCaretRect(D2D1_RECT_F& outRect)
{
	outRect = D2D1::RectF();
	if (!GetPresentationWindow()) return false;
	UpdateLayout();
	SyncBufferFromControlIfNeeded();
	const auto absolute = GetAbsoluteLocationDip();
	float x = Padding.Left;
	float y = Padding.Top;
	float height = GetRenderFont() ? GetRenderFont()->FontHeight : 16.0f;
	if (!buffer.empty())
	{
		if (_isVirtualized)
		{
			if (!GetCaretMetrics(_selectionEnd, x, y, height)) return false;
		}
		else
		{
			if (!_textLayoutCache || !GetRenderFont()) return false;
			const int caret = (std::clamp)(_selectionEnd, 0,
				static_cast<int>(buffer.size()));
			auto hit = GetRenderFont()->HitTestTextRange(
				_textLayoutCache, static_cast<UINT32>(caret), 0);
			if (hit.empty()) return false;
			x = hit[0].left + Padding.Left;
			y = hit[0].top + Padding.Top - _verticalScrollOffset;
			height = hit[0].height > 0.0f ? hit[0].height : height;
		}
	}
	outRect = D2D1::RectF(
		static_cast<float>(absolute.x) + x,
		static_cast<float>(absolute.y) + y,
		static_cast<float>(absolute.x) + x + 1.0f,
		static_cast<float>(absolute.y) + y + (std::max)(1.0f, height));
	return true;
}

bool RichTextBox::ApplyTextInput(const TextCompositionEventArgs& input)
{
	if (_isReadOnly || input.Text.empty()) return false;
	InputText(input.Text);
	UpdateScroll(_selectionEnd >= static_cast<int>(buffer.size()));
	InvalidateVisual();
	return true;
}

void RichTextBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;

	float renderHeight = TextViewportHeight();
	float maxScroll = _textSize.height - renderHeight;

	float scrollBlockHeight = (renderHeight / _textSize.height) * renderHeight;
	if (scrollBlockHeight < this->ActualHeight * 0.1f)scrollBlockHeight = this->ActualHeight * 0.1f;

	float scrollHeight = this->ActualHeight - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float thumbGrabOffset = std::clamp(_verticalScrollThumbGrabOffset, 0.0f, scrollBlockHeight);
	float targetTop = posY - thumbGrabOffset;
	float scrollRatio = targetTop / scrollHeight;
	scrollRatio = std::clamp(scrollRatio, 0.0f, 1.0f);
	float newScroll = scrollRatio * maxScroll;
	{
		this->_verticalScrollOffset = newScroll;
		if (this->_verticalScrollOffset < 0) this->_verticalScrollOffset = 0;
		if (this->_verticalScrollOffset > maxScroll + 1) this->_verticalScrollOffset = maxScroll + 1;
		InvalidateVisual();
	}
}
void RichTextBox::SetScrollByPos(float localY)
{
	const float renderHeight = TextViewportHeight();
	if (renderHeight <= 0.0f || _textSize.height <= 0.0f)
	{
		this->_verticalScrollOffset = 0.0f;
		return;
	}

	if (_textSize.height <= renderHeight)
	{
		this->_verticalScrollOffset = 0.0f;
		return;
	}

	const float maxScroll = std::max(0.0f, _textSize.height - renderHeight);

	float scrollBlockHeight = (renderHeight / _textSize.height) * renderHeight;
	if (scrollBlockHeight < this->ActualHeight * 0.1f) scrollBlockHeight = this->ActualHeight * 0.1f;
	if (scrollBlockHeight > static_cast<float>(this->ActualHeight)) scrollBlockHeight = static_cast<float>(this->ActualHeight);

	const float topPosition = scrollBlockHeight * 0.5f;
	const float bottomPosition = this->ActualHeight - topPosition;
	if (bottomPosition > topPosition)
	{
		const float percent = std::clamp((localY - topPosition) / (bottomPosition - topPosition), 0.0f, 1.0f);
		this->_verticalScrollOffset = maxScroll * percent;
	}
	this->_verticalScrollOffset = std::clamp(this->_verticalScrollOffset, 0.0f, maxScroll);
}
void RichTextBox::InputText(std::wstring input)
{
	SyncBufferFromControlIfNeeded();
	TrimToMaxLength();
	std::wstring oldText = this->buffer;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto result = CuiTextEdit::ReplaceSelection(this->buffer, this->_selectionStart, this->_selectionEnd, input, RichEditOptions());
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
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	SyncControlTextFromBuffer(oldText);
	NotifySelectionChanged();
}
void RichTextBox::InputBack()
{
	SyncBufferFromControlIfNeeded();
	std::wstring oldText = this->buffer;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto result = CuiTextEdit::Backspace(this->buffer, this->_selectionStart, this->_selectionEnd, RichEditOptions());
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
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	SyncControlTextFromBuffer(oldText);
	NotifySelectionChanged();
}
void RichTextBox::InputDelete()
{
	SyncBufferFromControlIfNeeded();
	std::wstring oldText = this->buffer;
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	auto result = CuiTextEdit::DeleteForward(this->buffer, this->_selectionStart, this->_selectionEnd, RichEditOptions());
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
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		this->undoStack.push_back(rec);
		this->redoStack.clear();
	}
	SyncControlTextFromBuffer(oldText);
	NotifySelectionChanged();
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
		this->_selectionStart = rec.selStartBefore;
		this->_selectionEnd = rec.selEndBefore;
	}
	else
	{
		this->_selectionStart = rec.selStartAfter;
		this->_selectionEnd = rec.selEndAfter;
	}
	TrimToMaxLength();
	this->_selectionStart = std::clamp(this->_selectionStart, 0, (int)this->buffer.size());
	this->_selectionEnd = std::clamp(this->_selectionEnd, 0, (int)this->buffer.size());
	this->selRangeDirty = true;
	this->blocksDirty = true;

	this->isApplyingUndoRedo = false;
	SyncControlTextFromBuffer(oldText);
	NotifySelectionChanged();
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
	if (this->_textLayoutDirty || (this->_isVirtualized && (this->blocksDirty || this->blockMetricsDirty)) || (!this->_isVirtualized && this->_textLayoutCache == nullptr))
	{
		this->UpdateLayout();
	}

	if (this->_isVirtualized)
	{
		float cx, cy, ch;
		if (GetCaretMetrics(this->_selectionEnd, cx, cy, ch))
		{
			float renderHeight = TextViewportHeight();
			float caretTopContent = (cy - Padding.Top)
				+ this->_verticalScrollOffset;
			float caretBottomContent = caretTopContent + ch;
			if (arrival && this->_selectionEnd >= (int)this->buffer.size())
			{
				const float maxScroll = std::max(0.0f, this->_textSize.height - renderHeight);
				this->_verticalScrollOffset = maxScroll;
			}
			else if (caretBottomContent - this->_verticalScrollOffset > renderHeight)
			{
				this->_verticalScrollOffset = caretBottomContent - renderHeight;
			}
			if (caretTopContent - this->_verticalScrollOffset < 0.0f)
				this->_verticalScrollOffset = caretTopContent;
			if (this->_verticalScrollOffset < 0) this->_verticalScrollOffset = 0;
		}
		return;
	}
	float renderHeight = TextViewportHeight();
	auto font = this->GetRenderFont();
	if (!font || !this->_textLayoutCache) return;
	auto selected = font->HitTestTextRange(this->_textLayoutCache, (UINT32)_selectionEnd, (UINT32)0);
	if (selected.size() > 0)
	{
		auto lastSelect = selected[0];
		if (arrival && this->_selectionEnd >= (int)this->buffer.size())
		{
			const float maxScroll = std::max(0.0f, this->_textSize.height - renderHeight);
			_verticalScrollOffset = maxScroll;
		}
		else if ((lastSelect.top + lastSelect.height) - _verticalScrollOffset > renderHeight)
		{
			_verticalScrollOffset = (lastSelect.top + lastSelect.height) - renderHeight;
		}
		if (lastSelect.top - _verticalScrollOffset < 0.0f)
		{
			_verticalScrollOffset = lastSelect.top;
		}
	}
}
void RichTextBox::AppendText(std::wstring str)
{
	SyncBufferFromControlIfNeeded();
	this->_selectionStart = this->_selectionEnd = (int)this->buffer.size();
	this->InputText(str);
	this->selRangeDirty = true;
}
void RichTextBox::AppendLine(std::wstring str)
{
	SyncBufferFromControlIfNeeded();
	this->_selectionStart = this->_selectionEnd = (int)this->buffer.size();
	this->InputText(str + L"\r\n");
	this->selRangeDirty = true;
}
std::wstring RichTextBox::GetSelectedString()
{
	SyncBufferFromControlIfNeeded();
	auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->buffer.size());
	if (!span.HasSelection())
		return L"";
	return this->buffer.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}

// ---- 公共选择/编辑 API ----
int RichTextBox::GetSelectionStart()
{
	SyncBufferFromControlIfNeeded();
	auto span = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, buffer.size());
	return span.start;
}

int RichTextBox::GetSelectionLength()
{
	auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->buffer.size());
	return span.HasSelection() ? static_cast<int>(span.Length()) : 0;
}

int RichTextBox::GetCaretIndex()
{
	SyncBufferFromControlIfNeeded();
	return (std::clamp)(
		_selectionEnd, 0, static_cast<int>(buffer.size()));
}

void RichTextBox::SetCaretIndex(int value)
{
	Select(value, 0);
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
	this->_selectionStart = start;
	this->_selectionEnd = start + length;
	this->selRangeDirty = true;
	NotifySelectionChanged();
	this->InvalidateVisual();
}

void RichTextBox::SelectAll()
{
	Select(0, static_cast<int>(this->Text.size()));
}

void RichTextBox::ClearSelection()
{
	this->_selectionEnd = this->_selectionStart;
	this->selRangeDirty = true;
	NotifySelectionChanged();
	this->InvalidateVisual();
}

void RichTextBox::Clear()
{
	if (_isReadOnly) return;
	this->SelectAll();
	this->InputBack();
}

void RichTextBox::InsertText(const std::wstring& text)
{
	if (_isReadOnly || (text.empty() && !HasSelection())) return;
	this->InputText(text);
}

void RichTextBox::InsertTextAndSelect(
	const std::wstring& text, int selectionStart, int selectionLength)
{
	if (_isReadOnly || (text.empty() && !HasSelection())) return;
	const size_t undoCount = this->undoStack.size();
	this->InputText(text);
	this->Select(selectionStart, selectionLength);
	if (this->undoStack.size() > undoCount)
	{
		auto& record = this->undoStack.back();
		record.selStartAfter = this->_selectionStart;
		record.selEndAfter = this->_selectionEnd;
	}
}

void RichTextBox::ReplaceAllTextAndSelect(
	const std::wstring& text, int selectionStart, int selectionLength)
{
	if (_isReadOnly) return;
	SyncBufferFromControlIfNeeded();
	const int selectionStartBefore = this->_selectionStart;
	const int selectionEndBefore = this->_selectionEnd;
	if (this->buffer == text)
	{
		this->Select(selectionStart, selectionLength);
		return;
	}

	this->SelectAll();
	const size_t undoCount = this->undoStack.size();
	this->InputText(text);
	this->Select(selectionStart, selectionLength);
	if (this->undoStack.size() > undoCount)
	{
		auto& record = this->undoStack.back();
		record.selStartBefore = selectionStartBefore;
		record.selEndBefore = selectionEndBefore;
		record.selStartAfter = this->_selectionStart;
		record.selEndAfter = this->_selectionEnd;
	}
}

bool RichTextBox::Copy()
{
	const std::wstring selected = this->GetSelectedString();
	if (selected.empty()) return false;
	return WriteClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, selected);
}

bool RichTextBox::Cut()
{
	if (_isReadOnly) return false;
	const std::wstring selected = this->GetSelectedString();
	if (selected.empty()) return false;
	if (!WriteClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, selected))
		return false;
	this->InputBack();
	return true;
}

bool RichTextBox::Paste()
{
	if (_isReadOnly) return false;
	std::wstring clipboardText;
	if (!TryReadClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, clipboardText))
		return false;
	if (clipboardText.empty()) return false;
	this->InputText(clipboardText);
	return true;
}

bool RichTextBox::CanPaste() const noexcept
{
	return !_isReadOnly
		&& ::IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
}

void RichTextBox::PreparePresentation()
{
	Control::PreparePresentation();
	UpdateLayout();
}

void RichTextBox::OnRender()
{
	if (this->IsVisible == false)return;
	bool isUnderMouse = this->IsMouseOver;
	auto d2d = this->GetDrawingContext();
	auto font = this->GetRenderFont();
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
		const float radius = (std::min)(
			_fallbackCornerRadius, actualHeight * 0.5f);
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, backColor, radius);
		if ((isUnderMouse || isSelected) && _fallbackHoverColor.a > 0.0f)
			d2d->FillRoundRect(1.0f, 1.0f,
				(std::max)(0.0f, actualWidth - 2.0f),
				(std::max)(0.0f, actualHeight - 2.0f),
				_fallbackHoverColor, (std::max)(0.0f, radius - 1.0f));
		if (this->buffer.size() > 0)
		{
			if (this->_isVirtualized)
			{
				float renderWidth = TextViewportWidth();
				float renderHeight = TextViewportHeight();
				if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

				int sels = std::min(_selectionStart, _selectionEnd);
				int sele = std::max(_selectionStart, _selectionEnd);
				int selLen = sele - sels;

				float cx, cy, ch;
				if (isSelected && selLen == 0 && GetCaretMetrics(this->_selectionEnd, cx, cy, ch))
				{
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

				float viewTop = this->_verticalScrollOffset;
				float viewBottom = this->_verticalScrollOffset + renderHeight;

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
					float drawY = Padding.Top
						+ (top - this->_verticalScrollOffset);
					float drawX = Padding.Left;

					if (isSelected && !highlightRanges.empty())
					{
						const int blockStart = static_cast<int>(blocks[i].start);
						const int blockEnd = static_cast<int>(
							blocks[i].start + blocks[i].len);
						for (const auto& highlight : highlightRanges)
						{
							const int rangeStart = (std::max)(
								highlight.Start, blockStart);
							const int rangeEnd = (std::min)(
								highlight.Start + highlight.Length, blockEnd);
							if (rangeEnd <= rangeStart) continue;
							auto ranges = font->HitTestTextRange(
								blocks[i].layout,
								static_cast<UINT32>(rangeStart - blockStart),
								static_cast<UINT32>(rangeEnd - rangeStart));
							for (const auto& range : ranges)
							{
								d2d->FillRect(range.left + drawX,
									range.top + drawY, range.width, range.height,
									_highlightBackColor);
							}
						}
					}

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
									_selectionBackColor);
							}
						}
					}

					ApplyTextDrawingEffects(
						blocks[i].layout,
						static_cast<int>(blocks[i].start),
						static_cast<int>(blocks[i].len),
						isSelected);
					d2d->DrawStringLayout(this->blocks[i].layout, drawX, drawY, this->RendererForegroundColor);
				}
			}
			else if (isSelected)
			{
				ApplyTextDrawingEffects(
					_textLayoutCache, 0,
					static_cast<int>(buffer.size()), true);
				for (const auto& highlight : highlightRanges)
				{
					auto ranges = font->HitTestTextRange(
						_textLayoutCache,
						static_cast<UINT32>(highlight.Start),
						static_cast<UINT32>(highlight.Length));
					for (const auto& range : ranges)
					{
						d2d->FillRect(range.left + Padding.Left,
							range.top + Padding.Top - _verticalScrollOffset,
							range.width, range.height, _highlightBackColor);
					}
				}
				if (isSelected && this->selRangeDirty)
				{
					UpdateSelRange();
				}
				int sels = _selectionStart <= _selectionEnd ? _selectionStart : _selectionEnd;
				int sele = _selectionEnd >= _selectionStart ? _selectionEnd : _selectionStart;
				int selLen = sele - sels;
				if (selLen != 0)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(
							sr.left + Padding.Left,
							(sr.top + Padding.Top) - this->_verticalScrollOffset,
							sr.width,
							sr.height,
							_selectionBackColor);
					}
				}
				else
				{
					if (selLen == 0 && !selRange.empty())
					{
						const auto caret = selRange[0];
						const float lx = caret.left + Padding.Left;
						const float ly = (caret.top + Padding.Top)
							- this->_verticalScrollOffset;
						const float ah = caret.height > 0 ? caret.height : font->FontHeight;
						const auto absoluteLocation = this->GetAbsoluteLocationDip();
						this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + lx - 2.0f, static_cast<float>(absoluteLocation.y) + ly - 2.0f, static_cast<float>(absoluteLocation.x) + lx + 2.0f, static_cast<float>(absoluteLocation.y) + ly + ah + 2.0f };
						this->_caretRectCacheValid = true;
					}
					if (!selRange.empty())
					{
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + Padding.Left,
							(selRange[0].top + Padding.Top) - this->_verticalScrollOffset };
						caretEnd = { selRange[0].left + Padding.Left,
							(selRange[0].top + selRange[0].height + Padding.Top)
								- this->_verticalScrollOffset };
					}
				}
				d2d->DrawStringLayout(this->_textLayoutCache,
					Padding.Left, Padding.Top - this->_verticalScrollOffset,
					this->RendererForegroundColor);
			}
			else
			{
				ApplyTextDrawingEffects(
					_textLayoutCache, 0,
					static_cast<int>(buffer.size()), false);
				d2d->DrawStringLayout(this->_textLayoutCache,
					Padding.Left, Padding.Top - this->_verticalScrollOffset,
					this->RendererForegroundColor);
			}
		}
		else
		{
			if (isSelected)
			{
				const float lx = Padding.Left;
				const float ly = Padding.Top;
				const float ah = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				const auto absoluteLocation = this->GetAbsoluteLocationDip();
				this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + lx - 2.0f, static_cast<float>(absoluteLocation.y) + ly - 2.0f, static_cast<float>(absoluteLocation.x) + lx + 2.0f, static_cast<float>(absoluteLocation.y) + ly + ah + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { lx, ly };
				caretEnd = { lx, ly + 16.0f };
			}
		}
		UpdateCaretBlinkState(isSelected, this->_selectionStart, this->_selectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->RendererForegroundColor);
		}
		this->DrawScroll();
		const auto borderColor = isSelected
			? _fallbackFocusBorderColor : this->RendererBorderColor;
		const float borderWidth = isSelected
			? (std::max)(
				this->BorderThickness.MaxEdge(), _fallbackFocusBorder)
			: this->BorderThickness.MaxEdge();
		if (borderWidth > 0.0f && borderColor.a > 0.0f)
			d2d->DrawRoundRect(borderWidth * 0.5f, borderWidth * 0.5f,
				(std::max)(0.0f, actualWidth - borderWidth), (std::max)(0.0f, actualHeight - borderWidth),
				borderColor, borderWidth, radius);
	}
	if (!this->IsEnabled)
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight,
			_fallbackDisabledOverlayColor,
			(std::min)(_fallbackCornerRadius, actualHeight * 0.5f));
	}
	this->EndRender();
}

bool RichTextBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}
bool RichTextBox::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;
	SelectionNotificationScope selectionNotification{ this };
	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
	{
		if (input.WheelDelta > 0)
		{
			if (this->_verticalScrollOffset > 0)
			{
				this->_verticalScrollOffset -= 10;
				if (this->_verticalScrollOffset < 0)this->_verticalScrollOffset = 0;
				this->InvalidateVisual();
			}
		}
		else
		{
			auto font = this->GetRenderFont();
			float renderWidth = TextViewportWidth();
			float renderHeight = TextViewportHeight();
			if (_textSize.height > renderHeight) renderWidth -= 8.0f;
			if (this->_verticalScrollOffset < _textSize.height - renderHeight)
			{
				this->_verticalScrollOffset += 10;
				if (this->_verticalScrollOffset > _textSize.height - renderHeight) this->_verticalScrollOffset = _textSize.height - renderHeight;
				this->InvalidateVisual();
			}
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case InputReportKind::PointerMove:
	{
		if (isDraggingScroll) {
			UpdateScrollDrag(static_cast<float>(input.Y));
		}
		if (input.IsButtonPressed(MouseButton::Left)
			&& this->GetPresentationWindow()->GetKeyboardFocusedElement() == this
			&& !isDraggingScroll)
		{
			auto font = this->GetRenderFont();
			if (this->_isVirtualized)
				_selectionEnd = HitTestGlobalIndex((float)input.X, (float)input.Y);
			else
				_selectionEnd = font->HitTestTextPosition(
					this->_textLayoutCache, input.X - Padding.Left,
					(input.Y + this->_verticalScrollOffset) - Padding.Top);
			UpdateScroll();
			this->InvalidateVisual();
			this->selRangeDirty = true;
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton == MouseButton::Left
			|| input.ChangedButton == MouseButton::Right)
		{
			if (input.ChangedButton == MouseButton::Left)
				(void)CaptureMouse();
			if (this->GetPresentationWindow()->GetKeyboardFocusedElement() != this)
			{
				auto previousSelection = this->GetPresentationWindow()->GetKeyboardFocusedElement();
				this->GetPresentationWindow()->SetKeyboardFocus(this, false);
				if (previousSelection) previousSelection->InvalidateVisual();
			}
			if (input.ChangedButton == MouseButton::Left
				&& input.X >= ActualWidth - 8.0f && input.X <= ActualWidth)
			{
				// 竖向滚动条：点在滑块上则用按下点锚定；否则用滑块中心（原行为）
				const float renderHeight = TextViewportHeight();
				if (renderHeight > 0.0f && _textSize.height > renderHeight)
				{
					const float maxScroll = std::max(0.0f, _textSize.height - renderHeight);
					float thumbHeight = (renderHeight / _textSize.height) * renderHeight;
					if (thumbHeight < this->ActualHeight * 0.1f) thumbHeight = this->ActualHeight * 0.1f;
					if (thumbHeight > static_cast<float>(this->ActualHeight)) thumbHeight = static_cast<float>(this->ActualHeight);
					const float moveSpace = std::max(0.0f, (float)this->ActualHeight - thumbHeight);
					float scrollRatio = 0.0f;
					if (maxScroll > 0.0f) scrollRatio = std::clamp(this->_verticalScrollOffset / maxScroll, 0.0f, 1.0f);
					const float thumbTop = scrollRatio * moveSpace;
					const float pointerY = (float)input.Y;
					const bool hitThumb = (pointerY >= thumbTop && pointerY <= (thumbTop + thumbHeight));
					_verticalScrollThumbGrabOffset = hitThumb ? (pointerY - thumbTop) : (thumbHeight * 0.5f);
				}
				else
				{
					_verticalScrollThumbGrabOffset = 0.0f;
				}
				isDraggingScroll = true;
				UpdateScrollDrag((float)input.Y);
				this->InvalidateVisual();
			}
			else
			{
				SyncBufferFromControlIfNeeded();
				auto font = this->GetRenderFont();
				const int hit = this->_isVirtualized
					? HitTestGlobalIndex((float)input.X, (float)input.Y)
					: font->HitTestTextPosition(this->_textLayoutCache,
						input.X - Padding.Left,
						(input.Y + this->_verticalScrollOffset) - Padding.Top);
				const auto selection = CuiTextEdit::NormalizeSelection(
					this->_selectionStart, this->_selectionEnd, this->buffer.size());
				if (input.ChangedButton == MouseButton::Left
					|| !selection.HasSelection()
					|| hit < selection.start || hit > selection.end)
				{
					this->_selectionStart = this->_selectionEnd = hit;
					this->selRangeDirty = true;
				}
			}
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::PointerUp:
	{
		if (isDraggingScroll) {
			isDraggingScroll = false;
		}
		else if (input.ChangedButton == MouseButton::Left
			&& this->GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		{
			auto font = this->GetRenderFont();
			if (this->_isVirtualized)
				_selectionEnd = HitTestGlobalIndex((float)input.X, (float)input.Y);
			else
				_selectionEnd = font->HitTestTextPosition(
					this->_textLayoutCache, input.X - Padding.Left,
					(input.Y + this->_verticalScrollOffset) - Padding.Top);
			this->selRangeDirty = true;
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
		isDraggingScroll = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		return Control::ProcessInput(input);
	case InputReportKind::PointerDoubleClick:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		this->GetPresentationWindow()->SetKeyboardFocus(this, false);
		SyncBufferFromControlIfNeeded();
		UpdateLayout();
		int hitIndex = 0;
		if (this->_isVirtualized)
			hitIndex = HitTestGlobalIndex((float)input.X, (float)input.Y);
		else
			hitIndex = this->GetRenderFont()->HitTestTextPosition(
				this->_textLayoutCache, input.X - Padding.Left,
				(input.Y + this->_verticalScrollOffset) - Padding.Top);
		hitIndex = std::clamp(hitIndex, 0, (int)this->buffer.size());
		this->_selectionStart = CuiTextEdit::GetLineStartIndex(this->buffer, hitIndex);
		this->_selectionEnd = CuiTextEdit::GetLineEndIndex(this->buffer, hitIndex);
		if (this->_selectionStart == this->_selectionEnd && this->_selectionEnd < (int)this->buffer.size())
			this->_selectionEnd = GetNextCaretIndex(this->_selectionEnd);
		this->selRangeDirty = true;
		UpdateScroll();
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDoubleClick(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::KeyDown:
	{
		bool handled = false;
		if (!_isReadOnly && input.Key == Key::Tab
			&& _acceptsTab)
		{
			this->InputText(L"\t");
			this->selRangeDirty = true;
			UpdateScroll();
			this->InvalidateVisual();
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
			if (!_isReadOnly)
			{
				InputBack();
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Return)
		{
			handled = true;
			if (!_isReadOnly)
			{
				InputText(L"\r\n");
				UpdateScroll(true);
			}
		}
		else if (input.Key == Key::Delete)
		{
			handled = true;
			if (!_isReadOnly)
			{
				this->InputDelete();
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Right)
		{
			handled = true;
			const bool extendSelection = input.HasModifier(ModifierKeys::Shift);
			auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->buffer.size());
			if (!extendSelection && span.HasSelection())
			{
				this->_selectionStart = this->_selectionEnd = span.end;
				this->selRangeDirty = true;
				UpdateScroll();
			}
			else if (this->_selectionEnd < (int)this->buffer.size())
			{
				this->_selectionEnd = GetNextCaretIndex(this->_selectionEnd);
				if (!extendSelection)
					this->_selectionStart = this->_selectionEnd;
				this->selRangeDirty = true;
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Left)
		{
			handled = true;
			const bool extendSelection = input.HasModifier(ModifierKeys::Shift);
			auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->buffer.size());
			if (!extendSelection && span.HasSelection())
			{
				this->_selectionStart = this->_selectionEnd = span.start;
				this->selRangeDirty = true;
				UpdateScroll();
			}
			else if (this->_selectionEnd > 0)
			{
				this->_selectionEnd = GetPreviousCaretIndex(this->_selectionEnd);
				if (!extendSelection)
					this->_selectionStart = this->_selectionEnd;
				this->selRangeDirty = true;
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Up)
		{
			handled = true;
			auto font = this->GetRenderFont();
			if (this->_isVirtualized)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->_selectionEnd, cx, cy, ch))
					this->_selectionEnd = HitTestGlobalIndex(cx, cy - font->FontHeight);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->_textLayoutCache, (UINT32)this->_selectionEnd, (UINT32)0);
				this->_selectionEnd = font->HitTestTextPosition(this->_textLayoutCache, hit[0].left, hit[0].top - (font->FontHeight * 0.5f));
			}
			if (!input.HasModifier(ModifierKeys::Shift))
			{
				this->_selectionStart = this->_selectionEnd;
			}
			if (this->_selectionEnd < 0)
			{
				this->_selectionEnd = 0;
			}
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (input.Key == Key::Down)
		{
			handled = true;
			auto font = this->GetRenderFont();
			if (this->_isVirtualized)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->_selectionEnd, cx, cy, ch))
					this->_selectionEnd = HitTestGlobalIndex(cx, cy + font->FontHeight);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->_textLayoutCache, (UINT32)this->_selectionEnd, (UINT32)0);
				this->_selectionEnd = font->HitTestTextPosition(this->_textLayoutCache, hit[0].left, hit[0].top + (font->FontHeight * 1.5f));
			}
			if (!input.HasModifier(ModifierKeys::Shift))
			{
				this->_selectionStart = this->_selectionEnd;
			}
			if (this->_selectionEnd > (int)this->buffer.size())
			{
				this->_selectionEnd = (int)this->buffer.size();
			}
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (input.Key == Key::Home)
		{
			handled = true;
			const bool controlDown = input.HasModifier(ModifierKeys::Control);
			this->_selectionEnd = controlDown ? 0 : CuiTextEdit::GetLineStartIndex(this->buffer, this->_selectionEnd);
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (input.Key == Key::End)
		{
			handled = true;
			const bool controlDown = input.HasModifier(ModifierKeys::Control);
			this->_selectionEnd = controlDown ? (int)this->buffer.size() : CuiTextEdit::GetLineEndIndex(this->buffer, this->_selectionEnd);
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			this->selRangeDirty = true;
			UpdateScroll();
		}
		else if (input.Key == Key::PageUp)
		{
			handled = true;
			auto font = this->GetRenderFont();
			if (this->_isVirtualized)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->_selectionEnd, cx, cy, ch))
					this->_selectionEnd = HitTestGlobalIndex(cx, cy - this->ActualHeight);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->_textLayoutCache, (UINT32)this->_selectionEnd, (UINT32)0);
				this->_selectionEnd = font->HitTestTextPosition(this->_textLayoutCache, hit[0].left, hit[0].top - this->ActualHeight);
			}
			if (!input.HasModifier(ModifierKeys::Shift))
			{
				this->_selectionStart = this->_selectionEnd;
			}
			if (this->_selectionEnd < 0)
			{
				this->_selectionEnd = 0;
			}
			this->selRangeDirty = true;
			UpdateScroll(true);
		}
		else if (input.Key == Key::PageDown)
		{
			handled = true;
			auto font = this->GetRenderFont();
			if (this->_isVirtualized)
			{
				float cx, cy, ch;
				if (GetCaretMetrics(this->_selectionEnd, cx, cy, ch))
					this->_selectionEnd = HitTestGlobalIndex(cx, cy + this->ActualHeight);
			}
			else
			{
				auto hit = font->HitTestTextRange(this->_textLayoutCache, (UINT32)this->_selectionEnd, (UINT32)0);
				this->_selectionEnd = font->HitTestTextPosition(this->_textLayoutCache, hit[0].left, hit[0].top + this->ActualHeight);
			}
			if (!input.HasModifier(ModifierKeys::Shift))
			{
				this->_selectionStart = this->_selectionEnd;
			}
			if (this->_selectionEnd > (int)this->buffer.size())
			{
				this->_selectionEnd = (int)this->buffer.size();
			}
			this->selRangeDirty = true;
			UpdateScroll(true);
		}
		else if (input.Key == Key::Escape)
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
