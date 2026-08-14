#pragma once
#include "TextBoxBase.h"
#include "Label.h"

#include <limits>

/** WPF TextBox.CharacterCasing input policy. */
enum class CharacterCasing : uint8_t
{
	Normal,
	Lower,
	Upper
};

/**
 * @file TextBox.h
 * @brief TextBox：WPF 文本输入控件（支持多行、换行、选择、滚动与 IME）。
 *
 * 关键字段：
 * - _selectionStart/_selectionEnd：选择区间（基于字符索引）
 * - _horizontalScrollOffset：水平滚动偏移（像素），用于长文本显示
 * - GetAnimatedInvalidRect：用于光标闪烁等动画区域增量刷新
 */
class TextBox : public TextBoxBase
{
protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TextBoxAutomationPeer>(*this, L"TextBox");
	}
	void VisitDeclaredInheritedProperties(
		void* context, InheritedPropertyVisitor visitor) const override;
private:
	static const DependencyPropertyMetadataRegistration&
		TextAlignmentMetadataRelation();
	static const DependencyPropertyMetadataRegistration&
		TextWrappingMetadataRelation();
	int _maxLength = 0;
	int _minLines = 1;
	int _maxLines = (std::numeric_limits<int>::max)();
	::TextAlignment _textAlignment = ::TextAlignment::Left;
	::TextWrapping _textWrapping = ::TextWrapping::NoWrap;
	::CharacterCasing _characterCasing =
		::CharacterCasing::Normal;
	D2D1_SIZE_F _textSize = { 0,0 };
	int _selectionStart = 0;
	int _selectionEnd = 0;
	float _horizontalScrollOffset = 0.0f;
	float _verticalScrollOffset = 0.0f;
	Microsoft::WRL::ComPtr<IDWriteTextLayout> _textLayout;
	std::wstring _layoutText;
	IDWriteTextFormat* _layoutFont = nullptr;
	float _layoutViewportWidth = -1.0f;
	::TextAlignment _layoutTextAlignment = ::TextAlignment::Left;
	::TextWrapping _layoutTextWrapping = ::TextWrapping::NoWrap;
public:
	using UIElement::OnTextChanged;
	using UIElement::SelectionChanged;
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	PROPERTY(int, MaxLength);
	GET(int, MaxLength);
	SET(int, MaxLength);
	PROPERTY(int, MinLines);
	GET(int, MinLines);
	SET(int, MinLines);
	PROPERTY(int, MaxLines);
	GET(int, MaxLines);
	SET(int, MaxLines);
	PROPERTY(::TextAlignment, TextAlignment);
	GET(::TextAlignment, TextAlignment);
	SET(::TextAlignment, TextAlignment);
	PROPERTY(::TextWrapping, TextWrapping);
	GET(::TextWrapping, TextWrapping);
	SET(::TextWrapping, TextWrapping);
	PROPERTY(::CharacterCasing, CharacterCasing);
	GET(::CharacterCasing, CharacterCasing);
	SET(::CharacterCasing, CharacterCasing);
	virtual UIClass Type();
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& TextProperty();
	static const DependencyProperty& MaxLengthProperty();
	static const DependencyProperty& MinLinesProperty();
	static const DependencyProperty& MaxLinesProperty();
	static const DependencyProperty& TextAlignmentProperty();
	static const DependencyProperty& TextWrappingProperty();
	static const DependencyProperty& CharacterCasingProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	std::wstring GetSemanticText() const override;
	CursorKind QueryCursor(int localX, int localY) override { (void)localX; (void)localY; return this->IsEnabled ? CursorKind::IBeam : CursorKind::Arrow; }
	bool HitTestChildren() const override { return false; }
	bool HandlesMouseWheel() const override { return true; }
	bool ShouldHitTestChildrenAt(
		int localX, int localY) const override;
	bool HandlesNavigationKey(Key key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	// ---- 公共选择/编辑 API（薄封装，复用内部编辑与 Undo 路径） ----
	/** WPF-style normalized selection start; mutation goes through Select. */
	int GetSelectionStart();
	__declspec(property(get = GetSelectionStart)) int SelectionStart;
	/** @brief 当前选中长度。 */
	int GetSelectionLength();
	__declspec(property(get = GetSelectionLength)) int SelectionLength;
	int GetCaretIndex();
	void SetCaretIndex(int value);
	__declspec(property(get = GetCaretIndex, put = SetCaretIndex)) int CaretIndex;
	float GetHorizontalOffset() const noexcept { return _horizontalScrollOffset; }
	__declspec(property(get = GetHorizontalOffset)) float HorizontalOffset;
	float GetVerticalOffset() const noexcept { return _verticalScrollOffset; }
	__declspec(property(get = GetVerticalOffset)) float VerticalOffset;
	/** @brief 是否存在选区。 */
	bool HasSelection();
	/** @brief 选中 [start, start+length)。 */
	void Select(int start, int length);
	/** WPF-style text hit testing used by editing hosts to place the caret. */
	int GetCharacterIndexFromPoint(float localX, float localY);
	/** @brief 全选。 */
	void SelectAll();
	/** @brief 清除选区（光标折叠到选择起点）。 */
	void ClearSelection();
	/** @brief 清空文本（可撤销）。 */
	void Clear();
	/** @brief 在当前光标处插入文本（替换选区，可撤销）。 */
	void InsertText(const std::wstring& text);
	/** @brief 复制选区到剪贴板。 */
	bool Copy();
	/** @brief 剪切选区到剪贴板（可撤销）。 */
	bool Cut();
	/** @brief 从剪贴板粘贴（替换选区，可撤销）。 */
	bool Paste();
	/** @brief 撤销。 */
	void Undo();
	/** @brief 重做。 */
	void Redo();
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
	TextBox();
protected:
	D2D1_RECT_F _caretRectCache = { 0,0,0,0 };
	bool _caretRectCacheValid = false;
private:
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
	bool isApplyingUndoRedo = false;
	int _lastNotifiedSelectionStart = 0;
	int _lastNotifiedSelectionEnd = 0;
	struct SelectionNotificationScope
	{
		ControlWeakReference owner;
		~SelectionNotificationScope()
		{
			if (auto* live = dynamic_cast<TextBox*>(owner.Get()))
				live->NotifySelectionChanged();
		}
	};
	void NotifySelectionChanged();
	void StoreUndoRecord(UndoRecord record);
	void StoreRedoRecord(UndoRecord record);
	void InvalidateTextLayout() noexcept;
	IDWriteTextLayout* EnsureTextLayout();
	float TextViewportWidth() noexcept;
	float TextViewportHeight() noexcept;
	float GetTextOriginY(IDWriteTextLayout* layout);
	bool GetCaretLayoutMetrics(
		int caretIndex, DWRITE_HIT_TEST_METRICS& metrics);
	int HitTestTextPosition(float localX, float localY);
	int GetVisualLineBoundary(bool lineEnd);
	int GetVerticalCaretIndex(float lineDelta);
	void UpdateClearButtonPresentation();
	void UpdateLineConstraintPresentation();
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void UpdateScroll(bool arrival = false);
	void ApplyUndoRecord(const UndoRecord& rec, bool isUndo);
	void OnUndoPolicyChanged() override;
	void OnScrollPolicyChanged() override;
public:
	/** @brief 返回当前选中的文本片段。 */
	std::wstring GetSelectedString();
	std::wstring GetSelectedText() { return GetSelectedString(); }
	void SetSelectedText(const std::wstring& value)
	{
		InputText(value);
	}
	__declspec(property(
		get = GetSelectedText,
		put = SetSelectedText)) std::wstring SelectedText;
protected:
	bool ApplyTextInput(const TextCompositionEventArgs& input) override;
	bool TryGetTextInputCaretRect(D2D1_RECT_F& outRect) override;
	void OnControlTemplatePresentationChanged() override;
	void PrepareMeasureCore(
		const cui::core::Constraints& available) override;
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
};
