#pragma once
#include "TextBoxBase.h"

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
	D2D1_COLOR_F ForegroundColor{};
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
class RichTextBox : public TextBoxBase
{
protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TextBoxAutomationPeer>(*this, L"RichTextBox");
	}
private:
	int _maxLength = 0;
	bool _enableVirtualization = true;
	size_t _virtualizeThreshold = 20000;
	size_t _blockCharCount = 4096;
	D2D1_COLOR_F _fallbackHoverColor = cui::theme::palette::SurfaceSubtle;
	D2D1_COLOR_F _highlightBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _fallbackFocusBorderColor = cui::theme::palette::Accent;
	D2D1_COLOR_F _scrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F _scrollForeColor = cui::theme::palette::ScrollThumb;
	D2D1_COLOR_F _fallbackDisabledOverlayColor =
		cui::theme::palette::DisabledOverlay;
	float _fallbackCornerRadius = 7.0f;
	float _fallbackFocusBorder = 1.6f;
	float TextViewportWidth()
	{
		return (std::max)(0.0f,
			ActualWidth - Padding.Left - Padding.Right);
	}
	float TextViewportHeight()
	{
		return (std::max)(0.0f,
			ActualHeight - Padding.Top - Padding.Bottom);
	}
	bool CanVerticallyScroll() const noexcept
	{
		return _verticalScrollBarVisibility
			!= ScrollBarVisibility::Disabled;
	}
	bool IsVerticalScrollBarVisible() noexcept
	{
		return _verticalScrollBarVisibility
				== ScrollBarVisibility::Visible
			|| (_verticalScrollBarVisibility
					== ScrollBarVisibility::Auto
				&& _textSize.height > (
					ActualHeight - Padding.Top - Padding.Bottom));
	}
	std::wstring buffer;
	bool bufferSyncedFromControl = false;
	bool _textLayoutDirty = true;
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

	bool isDraggingScroll = false;
	float _verticalScrollThumbGrabOffset = 0.0f;
	IDWriteTextLayout* _textLayoutCache = nullptr;
	std::vector<DWRITE_HIT_TEST_METRICS> selRange;
	bool selRangeDirty = true;
	cui::core::Size lastLayoutSize{};

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
	D2D1_SIZE_F _textSize = { 0,0 };
	int _selectionStart = 0;
	int _selectionEnd = 0;
	float _verticalScrollOffset = 0.0f;
public:
	using UIElement::OnTextChanged;
	using UIElement::SelectionChanged;
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	virtual UIClass Type();
	/**
	 * CUI compatibility Text identity. WPF RichTextBox exposes Document instead;
	 * this property remains until the document object model is implemented.
	 */
	static const DependencyProperty& TextProperty();
	static const DependencyProperty& MaxLengthProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	std::wstring GetSemanticText() const override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool HitTestChildren() const override { return false; }
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(Key key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	/** Maximum character count; zero means unlimited. */
	PROPERTY(int, MaxLength);
	GET(int, MaxLength);
	SET(int, MaxLength);
	RichTextBox();
	~RichTextBox() override;
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
	void ReleaseTextLayout() noexcept;
	void ReleaseBlocks();
	void EnsureBlockLayout(int blockIndex, float renderWidth, float renderHeight);
	void EnsureAllBlockMetrics(float renderWidth, float renderHeight);
	int HitTestGlobalIndex(float x, float y);
	bool GetCaretMetrics(int caretIndex, float& outX, float& outY, float& outH);
	int GetVisualLineBoundary(bool lineEnd);
	int GetVerticalCaretIndex(float lineDelta);
	void DrawScroll();
	void UpdateScrollDrag(float posY);
	void SetScrollByPos(float localY);
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void ApplyUndoRecord(const UndoRecord& rec, bool isUndo);
	void StoreUndoRecord(UndoRecord record);
	void StoreRedoRecord(UndoRecord record);
	void OnUndoPolicyChanged() override;
	void OnScrollPolicyChanged() override;
	void UpdateScroll(bool arrival = false);
	void UpdateLayout();
	void UpdateSelRange();
	void ApplyTextDrawingEffects(
		IDWriteTextLayout* layout,
		int textStart,
		int textLength,
		bool includeSelection,
		ID2D1Brush* selectionTextBrush);
	ID2D1SolidColorBrush* GetTextStyleBrush(D2D1_COLOR_F color);
public:
	/** @brief 追加文本（不自动换行）。 */
	void AppendText(std::wstring str);
	/** @brief 追加一行文本（通常会追加换行）。 */
	void AppendLine(std::wstring str);
	/** @brief 获取当前选择文本。 */
	std::wstring GetSelectedString();

	// ---- 公共选择/编辑 API（薄封装，复用内部编辑与 Undo 路径） ----
	int GetSelectionStart();
	int GetSelectionLength();
	__declspec(property(get = GetSelectionLength)) int SelectionLength;
	int GetCaretIndex();
	void SetCaretIndex(int value);
	__declspec(property(get = GetCaretIndex, put = SetCaretIndex)) int CaretIndex;
	float GetVerticalOffset() const noexcept { return _verticalScrollOffset; }
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
	bool CanUndo() const noexcept
	{
		return !_isReadOnly && _isUndoEnabled
			&& _undoLimit != 0 && !undoStack.empty();
	}
	bool CanRedo() const noexcept
	{
		return !_isReadOnly && _isUndoEnabled
			&& _undoLimit != 0 && !redoStack.empty();
	}
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
	bool TryGetTextInputCaretRect(D2D1_RECT_F& outRect) override;
	bool ApplyTextInput(const TextCompositionEventArgs& input) override;

protected:
	void NotifyDeviceResourcesInvalidated() noexcept override;
	void PreparePresentation() override;
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
public:
	/** @brief 滚动到末尾。 */
	void ScrollToEnd();
};
