#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

struct RichTextBoxTextRange
{
	int Start = 0;
	int Length = 0;
};

/** Non-editing foreground style applied by code-oriented RichTextBox hosts. */
struct RichTextBoxTextStyleRange
{
	int Start = 0;
	int Length = 0;
	D2D1_COLOR_F ForeColor{};
};

/**
 * @file RichTextBox.h
 * @brief RichTextBox：富文本/大文本输入控件（支持虚拟化渲染）。
 *
 * 设计要点：
 * - 内部维护编辑缓冲区，与 Control::Text 在需要时同步
 * - 支持多行、选择区间、滚动条与光标命中测试
 * - 可启用虚拟化：按块（BlockCharCount）构建多个 DWrite TextLayout，以降低超长文本开销
 */
class RichTextBox : public Control
{
private:
	std::wstring buffer;
	bool bufferSyncedFromControl = false;
	int _lastNotifiedSelectionStart = 0;
	int _lastNotifiedSelectionEnd = 0;
	void NotifySelectionChanged();
	struct SelectionNotificationScope
	{
		RichTextBox* owner = nullptr;
		~SelectionNotificationScope()
		{
			if (owner) owner->NotifySelectionChanged();
		}
	};
	::Font* _lastLayoutFont = nullptr;
	struct UndoRecord
	{
		int pos = 0;
		std::wstring removedText;
		std::wstring insertedText;
		int selStartBefore = 0;
		int selEndBefore = 0;
		int selStartAfter = 0;
		int selEndAfter = 0;
	};
	std::vector<UndoRecord> undoStack;
	std::vector<UndoRecord> redoStack;
	std::vector<RichTextBoxTextRange> highlightRanges;
	std::vector<RichTextBoxTextStyleRange> textStyleRanges;
	struct TextStyleBrush
	{
		D2D1_COLOR_F Color{};
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> Brush;
	};
	std::vector<TextStyleBrush> textStyleBrushes;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext> textStyleBrushDeviceContext;
	bool isApplyingUndoRedo = false;

	POINT selectedPos = { 0,0 };
	bool isDraggingScroll = false;
	float _verticalScrollThumbGrabOffset = 0.0f;
	IDWriteTextLayout* _textLayoutCache = nullptr;
	std::vector<DWRITE_HIT_TEST_METRICS> selRange;
	bool selRangeDirty = true;
	SIZE lastLayoutSize = { 0,0 };

	struct TextBlock
	{
		size_t start = 0;
		size_t len = 0;
		IDWriteTextLayout* layout = nullptr;
		float height = -1.0f;
	};
	std::vector<TextBlock> blocks;
	std::vector<float> blockTops;
	bool blocksDirty = true;
	bool blockMetricsDirty = true;
	bool _isVirtualized = false;
	bool layoutWidthHasScrollBar = false;
	float virtualTotalHeight = 0.0f;
	float _cachedRenderWidth = 0.0f;
public:
	bool IsAccessibilityReadOnly() const override { return ReadOnly; }
	virtual UIClass Type();
	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	/** @brief 当前文本测量尺寸缓存（供渲染/布局使用）。 */
	D2D1_SIZE_F textSize = { 0,0 };
	/** @brief 鼠标悬停时背景色（实现可能会用到）。 */
	D2D1_COLOR_F UnderMouseColor = cui::theme::palette::SurfaceSubtle;
	/** @brief 选区背景色。 */
	D2D1_COLOR_F SelectedBackColor = cui::theme::palette::SelectionBack;
	/** @brief 选区前景色。 */
	D2D1_COLOR_F SelectedForeColor = cui::theme::palette::TextPrimary;
	/** Secondary non-editing highlights, for example paired markup names. */
	D2D1_COLOR_F HighlightBackColor = cui::theme::palette::AccentSelected;
	/** @brief 获得焦点时高亮色。 */
	D2D1_COLOR_F FocusedColor = cui::theme::palette::Surface;
	/** @brief 滚动条背景色。 */
	D2D1_COLOR_F ScrollBackColor = cui::theme::palette::ScrollTrack;
	/** @brief 滚动条前景色。 */
	D2D1_COLOR_F ScrollForeColor = cui::theme::palette::ScrollThumb;
	D2D1_COLOR_F DisabledOverlayColor = cui::theme::palette::DisabledOverlay;
	/** @brief 是否允许多行输入。 */
	bool AllowMultiLine = false;
	/** @brief 是否允许输入 Tab 字符。 */
	bool AllowTabInput = false;
	/** @brief 只读模式：允许选择/复制/滚动，但禁止用户修改文本。 */
	bool ReadOnly = false;
	/** @brief 最大文本长度（超出会被截断）。 */
	size_t MaxTextLength = 0;
	/** @brief 是否启用虚拟化（用于长文本）。 */
	bool EnableVirtualization = true;
	/** @brief 超过该字符数时进入虚拟化模式。 */
	size_t VirtualizeThreshold = 20000;
	/** @brief 每个虚拟化块的字符数。 */
	size_t BlockCharCount = 4096;
	/** @brief 选择起始索引（基于字符）。 */
	int SelectionStart = 0;
	/** @brief 选择结束索引（基于字符）。 */
	int SelectionEnd = 0;
	/** Raised once whenever the effective selection or caret changes. */
	SelectionChangedEvent OnSelectionChanged;
	/** @brief 边框宽度（像素）。 */
	float BorderThickness = 1.5f;
	/** @brief 圆角半径。 */
	float CornerRadius = 7.0f;
	/** @brief 聚焦时边框宽度。 */
	float FocusBorder = 1.6f;
	/** @brief 垂直滚动偏移（像素）。 */
	float VerticalScrollOffset = 0.0f;
	/** @brief 文本内边距（像素）。 */
	float TextMargin = 5.0f;
	/** @brief 创建富文本框。 */
	RichTextBox(std::wstring text, int x, int y, int width = 120, int height = 24);
private:
	D2D1_RECT_F _caretRectCache = { 0,0,0,0 };
	bool _caretRectCacheValid = false;
private:
	void SyncBufferFromControlIfNeeded();
	void SyncControlTextFromBuffer(const std::wstring& oldText);
	std::wstring NormalizeLineBreaks(const std::wstring& text) const;
	bool HasCrLfAt(int index) const;
	bool IsCaretBetweenCrLf(int index) const;
	int GetNextCaretIndex(int index) const;
	int GetPreviousCaretIndex(int index) const;
	void NormalizeSelectionRangeForErase(int& start, int& end) const;
	bool GetBackspaceEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const;
	bool GetDeleteEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const;
	void TrimToMaxLength();
	void RebuildBlocks();
	void ReleaseBlocks();
	void EnsureBlockLayout(int blockIndex, float renderWidth, float renderHeight);
	void EnsureAllBlockMetrics(float renderWidth, float renderHeight);
	int HitTestGlobalIndex(float x, float y);
	bool GetCaretMetrics(int caretIndex, float& outX, float& outY, float& outH);
	void DrawScroll();
	void UpdateScrollDrag(float posY);
	void SetScrollByPos(float localY);
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void ApplyUndoRecord(const UndoRecord& rec, bool isUndo);
	void UpdateScroll(bool arrival = false);
	void UpdateLayout();
	void UpdateSelRange();
	void ApplyTextDrawingEffects(
		IDWriteTextLayout* layout,
		int textStart,
		int textLength,
		bool includeSelection);
	ID2D1SolidColorBrush* GetTextStyleBrush(D2D1_COLOR_F color);
public:
	/** @brief 追加文本（不自动换行）。 */
	void AppendText(std::wstring str);
	/** @brief 追加一行文本（通常会追加换行）。 */
	void AppendLine(std::wstring str);
	/** @brief 获取当前选择文本。 */
	std::wstring GetSelectedString();

	// ---- 公共选择/编辑 API（薄封装，复用内部编辑与 Undo 路径） ----
	int GetSelectionLength();
	__declspec(property(get = GetSelectionLength)) int SelectionLength;
	bool HasSelection();
	void Select(int start, int length);
	void SelectAll();
	void ClearSelection();
	void Clear();
	void InsertText(const std::wstring& text);
	/** Replaces the selection as one Undo record and stores its final selection. */
	void InsertTextAndSelect(
		const std::wstring& text, int selectionStart, int selectionLength);
	/** Replaces the complete document as one Undo record while preserving the
	 *  pre-replacement selection for Undo and the requested selection for Redo. */
	void ReplaceAllTextAndSelect(
		const std::wstring& text, int selectionStart, int selectionLength);
	bool Copy();
	bool Cut();
	bool Paste();
	bool CanUndo() const noexcept { return !ReadOnly && !undoStack.empty(); }
	bool CanRedo() const noexcept { return !ReadOnly && !redoStack.empty(); }
	bool CanPaste() const noexcept;
	void Undo();
	void Redo();
	/** Scrolls the current caret/selection endpoint into the viewport. */
	void ScrollSelectionIntoView();
	/** Replaces the secondary, non-editing text highlights. */
	void SetHighlightRanges(std::vector<RichTextBoxTextRange> ranges);
	void ClearHighlightRanges();
	/** Replaces non-editing foreground styles without changing text or history. */
	void SetTextStyleRanges(std::vector<RichTextBoxTextStyleRange> ranges);
	void ClearTextStyleRanges();
	const std::vector<RichTextBoxTextStyleRange>& GetTextStyleRanges() const noexcept
	{
		return textStyleRanges;
	}
	/** Returns the current caret rectangle in top-level client DIPs. */
	bool TryGetCaretViewportRect(D2D1_RECT_F& outRect);

	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
	/** @brief 滚动到末尾。 */
	void ScrollToEnd();
};
