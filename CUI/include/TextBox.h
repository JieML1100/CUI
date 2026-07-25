#pragma once
#include "Control.h"

/**
 * @file TextBox.h
 * @brief TextBox：单行文本输入控件（支持选择、光标、滚动、IME）。
 *
 * 关键字段：
 * - _selectionStart/_selectionEnd：选择区间（基于字符索引）
 * - _horizontalScrollOffset：水平滚动偏移（像素），用于长文本显示
 * - GetAnimatedInvalidRect：用于光标闪烁等动画区域增量刷新
 */
class TextBox : public Control
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<TextBoxAutomationPeer>(*this, L"TextBox");
	}
private:
	cui::drawing::Brush _selectionBrush =
		cui::drawing::MakeSolidColorBrush(cui::theme::palette::Accent);
	double _selectionOpacity = 0.4;
	cui::drawing::Brush _selectionTextBrush =
		cui::drawing::MakeSolidColorBrush(cui::theme::palette::OnAccent);
	cui::drawing::Brush _caretBrush =
		cui::drawing::MakeSolidColorBrush(cui::theme::palette::TextPrimary);
	D2D1_SIZE_F _textSize = { 0,0 };
	int _selectionStart = 0;
	int _selectionEnd = 0;
	float _horizontalScrollOffset = 0.0f;
public:
	using UIElement::OnTextChanged;
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	virtual UIClass Type();
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
	CursorKind QueryCursor(int localX, int localY) override { (void)localX; (void)localY; return this->IsEnabled ? CursorKind::IBeam : CursorKind::Arrow; }
	bool HandlesMouseWheel() const override { return true; }
	bool HandlesNavigationKey(Key key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	PROPERTY(cui::drawing::Brush, SelectionBrush);
	GET(cui::drawing::Brush, SelectionBrush);
	SET(cui::drawing::Brush, SelectionBrush);
	PROPERTY(double, SelectionOpacity);
	GET(double, SelectionOpacity);
	SET(double, SelectionOpacity);
	PROPERTY(cui::drawing::Brush, SelectionTextBrush);
	GET(cui::drawing::Brush, SelectionTextBrush);
	SET(cui::drawing::Brush, SelectionTextBrush);
	PROPERTY(cui::drawing::Brush, CaretBrush);
	GET(cui::drawing::Brush, CaretBrush);
	SET(cui::drawing::Brush, CaretBrush);
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
	/** @brief 是否存在选区。 */
	bool HasSelection();
	/** @brief 选中 [start, start+length)。 */
	void Select(int start, int length);
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
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void UpdateScroll(bool arrival = false);
	void ApplyUndoRecord(const UndoRecord& rec, bool isUndo);
public:
	/** @brief 返回当前选中的文本片段。 */
	std::wstring GetSelectedString();
protected:
	bool ApplyTextInput(const TextCompositionEventArgs& input) override;
	bool TryGetTextInputCaretRect(D2D1_RECT_F& outRect) override;
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
};
