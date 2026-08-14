#pragma once
#include "FlowDocument.h"
#include "TextRange.h"
#include "TextBoxBase.h"

class RichTextBox;

/** Stable selection facade owned by one RichTextBox. */
class TextSelection final : public TextRange
{
public:
	TextPointer GetStart() const override;
	TextPointer GetEnd() const override;
	TextPointer GetAnchorPosition() const;
	TextPointer GetMovingPosition() const;
	__declspec(property(get = GetAnchorPosition)) TextPointer AnchorPosition;
	__declspec(property(get = GetMovingPosition)) TextPointer MovingPosition;
	void Select(
		const TextPointer& anchorPosition,
		const TextPointer& movingPosition) override;
	void SetText(std::wstring value) override;
	bool ApplyPropertyValue(
		const DependencyProperty& property,
		const BindingValue& value) override;
	TextSelectionPropertyValue GetPropertyValue(
		const DependencyProperty& property) const override;
	void ClearAllProperties() override;

private:
	explicit TextSelection(RichTextBox& owner) noexcept : _owner(&owner) {}
	RichTextBox* _owner = nullptr;
	friend class RichTextBox;
};

struct RichTextBoxTextRange
{
	int Start = 0;
	int Length = 0;
};

/**
 * @file RichTextBox.h
 * @brief RichTextBox：富文本/大文本输入控件（支持虚拟化渲染）。
 *
 * 设计要点：
 * - 内部维护编辑缓冲区，与 Control::Text 在需要时同步
 * - 支持多行、选择区间、滚动条与光标命中测试
 * - 大文档先在硬换行边界分段，再由 DirectWrite 渐进确定完整视觉行
 * - 只驻留视口附近的单行 layout，避免超长段落反复创建整段布局
 */
class RichTextBox : public TextBoxBase
{
	friend class FlowDocument;
	friend class TextRange;
	friend class TextSelection;
	friend struct RichTextBoxDocumentTestAccess;
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
	std::unique_ptr<FlowDocument> _document;
	bool _documentIsImplicit = false;
	RichTextDocument _flatDocument;
	bool _projectingDocumentText = false;
	bool _mutatingDocumentFromEditor = false;
	std::optional<RichTextCharacterStyle> _typingStyle;
	TextSelection _selection;
	bool _textLayoutDirty = true;
	int _editorNotificationDepth = 0;
	bool _pendingTextChanged = false;
	std::wstring _pendingTextChangedOldValue;
	std::wstring _pendingTextChangedNewValue;
	bool _pendingSelectionChanged = false;
	bool _historyVisibilityOverrideActive = false;
	bool _historyVisibilityCanUndo = false;
	bool _historyVisibilityCanRedo = false;
	int _lastNotifiedSelectionStart = 0;
	int _lastNotifiedSelectionEnd = 0;
	void BeginEditorNotificationTransaction() noexcept;
	void EndEditorNotificationTransaction(bool publish);
	void NotifyTextChanged(
		const std::wstring& oldText, const std::wstring& newText);
	void NotifySelectionChanged();
	void PublishSelectionChanged();
	struct EditorNotificationScope
	{
		explicit EditorNotificationScope(RichTextBox& ownerValue) noexcept
			: owner(&ownerValue)
		{
			owner->BeginEditorNotificationTransaction();
		}
		EditorNotificationScope(const EditorNotificationScope&) = delete;
		EditorNotificationScope& operator=(
			const EditorNotificationScope&) = delete;
		~EditorNotificationScope()
		{
			if (owner) owner->EndEditorNotificationTransaction(false);
		}
		void Commit()
		{
			auto* current = owner;
			owner = nullptr;
			current->EndEditorNotificationTransaction(true);
		}
		RichTextBox* owner = nullptr;
	};
	struct HistoryVisibilityScope
	{
		HistoryVisibilityScope(
			RichTextBox& ownerValue, bool canUndo, bool canRedo) noexcept
			: owner(ownerValue), previousActive(
				ownerValue._historyVisibilityOverrideActive),
			previousCanUndo(ownerValue._historyVisibilityCanUndo),
			previousCanRedo(ownerValue._historyVisibilityCanRedo)
		{
			owner._historyVisibilityOverrideActive = true;
			owner._historyVisibilityCanUndo = canUndo;
			owner._historyVisibilityCanRedo = canRedo;
		}
		~HistoryVisibilityScope()
		{
			owner._historyVisibilityOverrideActive = previousActive;
			owner._historyVisibilityCanUndo = previousCanUndo;
			owner._historyVisibilityCanRedo = previousCanRedo;
		}
		RichTextBox& owner;
		bool previousActive = false;
		bool previousCanUndo = false;
		bool previousCanRedo = false;
	};
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
		RichTextDocumentFragment removedFragment;
		RichTextDocumentFragment insertedFragment;
		std::optional<RichTextCharacterStyle> typingStyleBefore;
		std::optional<RichTextCharacterStyle> typingStyleAfter;
		int selStartBefore = 0;
		int selEndBefore = 0;
		int selStartAfter = 0;
		int selEndAfter = 0;
		LogicalDirection caretDirectionBefore = LogicalDirection::Forward;
		LogicalDirection caretDirectionAfter = LogicalDirection::Forward;
	};
	struct DocumentProjectionTransactionState
	{
		bool ExternalReset = false;
		bool PreviousProjectionTransactionActive = false;
		int EditorNotificationDepth = 0;
		bool PendingTextChanged = false;
		std::wstring PendingTextChangedOldValue;
		std::wstring PendingTextChangedNewValue;
		bool PendingSelectionChanged = false;
		std::wstring Buffer;
		std::wstring CompatibilityText;
		bool BufferSyncedFromControl = false;
		RichTextDocument FlatDocument;
		int SelectionStart = 0;
		int SelectionEnd = 0;
		LogicalDirection CaretDirection = LogicalDirection::Forward;
		std::optional<float> SuggestedCaretX;
		int SuggestedCaretIndex = -1;
		int LastNotifiedSelectionStart = 0;
		int LastNotifiedSelectionEnd = 0;
		std::optional<RichTextCharacterStyle> TypingStyle;
		std::vector<UndoRecord> UndoStack;
		std::vector<UndoRecord> RedoStack;
		std::vector<RichTextBoxTextRange> HighlightRanges;
	};
	std::vector<UndoRecord> undoStack;
	std::vector<UndoRecord> redoStack;
	std::vector<RichTextBoxTextRange> highlightRanges;
	std::vector<Microsoft::WRL::ComPtr<ID2D1Brush>> richTextStyleBrushes;
	bool isApplyingUndoRedo = false;
	bool _documentProjectionTransactionActive = false;

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
		size_t layoutLen = 0;
		size_t sentinelStyleIndex = 0;
		IDWriteTextLayout* layout = nullptr;
		float height = -1.0f;
		bool appendSentinel = false;
		bool singleVisualLine = false;
		::TextAlignment textAlignment = ::TextAlignment::Left;
		::FlowDirection flowDirection = ::FlowDirection::LeftToRight;
	};
	std::vector<TextBlock> blocks;
	std::vector<float> blockTops;
	bool blocksDirty = true;
	bool blockMetricsDirty = true;
	bool _isVirtualized = false;
	bool _hasMixedParagraphAlignment = false;
	::TextAlignment _uniformTextAlignment = ::TextAlignment::Left;
	bool _hasMixedParagraphFlowDirection = false;
	::FlowDirection _uniformFlowDirection =
		::FlowDirection::LeftToRight;
	bool layoutWidthHasScrollBar = false;
	float virtualTotalHeight = 0.0f;
	float _cachedRenderWidth = 0.0f;
	D2D1_SIZE_F _textSize = { 0,0 };
	int _selectionStart = 0;
	int _selectionEnd = 0;
	LogicalDirection _caretLogicalDirection = LogicalDirection::Forward;
	std::optional<float> _suggestedCaretX;
	int _suggestedCaretIndex = -1;
	float _verticalScrollOffset = 0.0f;
public:
	using UIElement::OnTextChanged;
	using UIElement::SelectionChanged;
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	virtual UIClass Type();
	/** Plain-text compatibility projection of Document; assigning it replaces
	 *  the document contents and clears direct character formatting/history. */
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
	FlowDocument& GetDocument() noexcept { return *_document; }
	const FlowDocument& GetDocument() const noexcept { return *_document; }
	void SetDocument(std::unique_ptr<FlowDocument> document);
	TextSelection& GetSelection() noexcept { return _selection; }
	const TextSelection& GetSelection() const noexcept { return _selection; }
private:
	struct DocumentProjectionTransactionState;
	D2D1_RECT_F _caretRectCache = { 0,0,0,0 };
	bool _caretRectCacheValid = false;
private:
	void SyncBufferFromControlIfNeeded();
	void SetDocumentCore(
		std::unique_ptr<FlowDocument> document, bool isImplicit);
	void OnCompatibilityTextChanged(
		const std::wstring& oldText, const std::wstring& newText);
	void OnDocumentChanged(bool resetEditorState);
	void OnFlowDocumentChangedInternal();
	std::unique_ptr<DocumentProjectionTransactionState>
		BeginFlowDocumentProjectionTransaction();
	void CommitFlowDocumentProjectionTransaction(
		DocumentProjectionTransactionState& state);
	void RollbackFlowDocumentProjectionTransaction(
		DocumentProjectionTransactionState& state) noexcept;
	void ReplaceDocumentContent(
		const RichTextDocumentFragment& fragment,
		bool editorMutation,
		std::optional<TextPointerTextChange> textChange = std::nullopt);
	void RebaseSelectionForDocumentChange(
		const TextPointerTextChange& textChange,
		std::size_t newTextLength) noexcept;
	void SelectPointers(
		const TextPointer& anchorPosition,
		const TextPointer& movingPosition);
	RichTextDocumentChange ReplaceRangeWithFragment(
		int start,
		int end,
		RichTextDocumentFragment fragment,
		bool collapseSelection = true);
	RichTextDocumentChange ReplaceTextRangeContent(
		std::size_t start,
		std::size_t length,
		RichTextDocumentFragment fragment);
	bool ApplyTextRangeFormat(
		std::size_t start,
		std::size_t length,
		const RichTextFormatDelta& delta);
	bool ApplyTextRangeParagraphFormat(
		std::size_t start,
		std::size_t length,
		const RichTextParagraphFormatDelta& delta);
	bool ApplySelectionFormat(const RichTextFormatDelta& delta);
	bool ApplySelectionParagraphFormat(
		const RichTextParagraphFormatDelta& delta);
	bool ResetSelectionFormattingCommand();
	bool AdjustSelectionFontSize(float amount);
	bool MoveSelectionByCharacter(bool forward, bool extendSelection);
	bool MoveSelectionByWord(bool forward, bool extendSelection);
	bool LogicalForwardForHorizontalArrow(bool right) const;
	::FlowDirection GetParagraphFlowDirectionAt(int caretIndex) const;
	bool MoveSelectionVertically(bool down, bool extendSelection);
	bool MoveSelectionByPage(bool down, bool extendSelection);
	bool MoveSelectionByParagraph(bool down, bool extendSelection);
	bool MoveSelectionToLineBoundary(bool lineEnd, bool extendSelection);
	bool MoveSelectionToDocumentBoundary(bool documentEnd, bool extendSelection);
	static void EnsureEditingCommandBindingsRegistered();
	bool ExecuteEditingCommand(const RoutedCommand& command);
	TextSelectionPropertyValue QuerySelectionProperty(
		const DependencyProperty& property) const;
	void ClearSelectionFormatting();
	RichTextCharacterStyle EffectiveTypingStyle() const;
	std::wstring NormalizeLineBreaks(const std::wstring& text) const;
	bool HasCrLfAt(int index) const;
	bool IsCaretBetweenCrLf(int index) const;
	int SnapCaretIndex(
		int index, LogicalDirection direction) const noexcept;
	int GetNextCaretIndex(int index) const;
	int GetPreviousCaretIndex(int index) const;
	void NormalizeSelectionRangeForErase(int& start, int& end) const;
	bool GetBackspaceEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const;
	bool GetDeleteEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const;
	void TrimToMaxLength();
	void RebuildBlocks();
	void UpdateParagraphLayoutState();
	void SplitLongBlocksIntoVisualLines(float renderWidth);
	void ApplySentinelLayoutFormatting(
		IDWriteTextLayout* layout,
		UINT32 layoutPosition,
		size_t sourceStyleIndex);
	void ReleaseTextLayout() noexcept;
	void ReleaseBlocks();
	void EnsureBlockLayout(int blockIndex, float renderWidth, float renderHeight);
	void EnsureAllBlockMetrics(float renderWidth, float renderHeight);
	int HitTestGlobalIndex(float x, float y);
	bool GetCaretMetrics(int caretIndex, float& outX, float& outY, float& outH);
	bool GetCaretMetrics(
		int caretIndex, LogicalDirection direction,
		float& outX, float& outY, float& outH);
	int GetCaretBlockIndex(
		int caretIndex, LogicalDirection direction) const noexcept;
	int GetVisualLineBoundary(int caretIndex, bool lineEnd);
	int GetVerticalCaretIndex(
		int caretIndex, bool down,
		LogicalDirection& targetDirection);
	int GetPageCaretIndex(int caretIndex, bool down);
	void ClearSuggestedCaretX() noexcept;
	bool TryGetSuggestedCaretX(
		int caretIndex, float& outX, float& outY, float& outHeight);
	int GetParagraphNavigationTarget(
		int caretIndex, bool down,
		bool preferPreviousAtBoundary) const;
	void DrawScroll();
	void UpdateScrollDrag(float posY);
	void SetScrollByPos(float localY);
	void InputText(std::wstring input);
	void InputLineBreak();
	void InputFragment(RichTextDocumentFragment fragment);
	void InputBack(bool byWord = false);
	void InputDelete(bool byWord = false);
	bool ApplyUndoRecord(const UndoRecord& rec, bool isUndo);
	void StoreUndoRecord(UndoRecord record);
	void StoreRedoRecord(UndoRecord record);
	RichTextCharacterStyle ResolveEffectiveCharacterStyle(
		RichTextCharacterStyle style) const;
	RichTextDocumentFragment CreateClipboardFragment() const;
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
	void ApplyTextLayoutFormatting(
		IDWriteTextLayout* layout, int textStart, int textLength);
	void DrawTextStyleBackgrounds(
		IDWriteTextLayout* layout, int textStart, int textLength,
		float drawX, float drawY);
	ID2D1Brush* CreateRichTextStyleBrush(
		const cui::drawing::Brush& definition);
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
			&& _undoLimit != 0
			&& (_historyVisibilityOverrideActive
				? _historyVisibilityCanUndo : !undoStack.empty());
	}
	bool CanRedo() const noexcept
	{
		return !_isReadOnly && _isUndoEnabled
			&& _undoLimit != 0
			&& (_historyVisibilityOverrideActive
				? _historyVisibilityCanRedo : !redoStack.empty());
	}
	bool CanPaste() const noexcept;
	void Undo();
	void Redo();
	/** Scrolls the current caret/selection endpoint into the viewport. */
	void ScrollSelectionIntoView();
	/** Replaces the secondary, non-editing text highlights. */
	void SetHighlightRanges(std::vector<RichTextBoxTextRange> ranges);
	void ClearHighlightRanges();
	const std::vector<RichTextBoxTextRange>& GetHighlightRanges() const noexcept
	{
		return highlightRanges;
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
