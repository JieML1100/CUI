#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file TextBox.h
 * @brief TextBox：单行文本输入控件（支持选择、光标、滚动、IME）。
 *
 * 关键字段：
 * - SelectionStart/SelectionEnd：选择区间（基于字符索引）
 * - OffsetX：水平滚动偏移（像素），用于长文本显示
 * - GetAnimatedInvalidRect：用于光标闪烁等动画区域增量刷新
 */
class TextBox : public Control
{
public:
	virtual UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override { (void)xof; (void)yof; return this->Enable ? CursorKind::IBeam : CursorKind::Arrow; }
	bool HandlesMouseWheel() const override { return true; }
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	/** @brief 当前文本测量尺寸缓存（像素）。 */
	D2D1_SIZE_F textSize = { 0,0 };
	D2D1_COLOR_F UnderMouseColor = Colors::White;
	D2D1_COLOR_F SelectedBackColor = { 0.f , 0.f , 1.f , 0.5f };
	D2D1_COLOR_F SelectedForeColor = Colors::White;
	D2D1_COLOR_F FocusedColor = Colors::White;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	/** @brief 选择起点（含）。 */
	int SelectionStart = 0;
	/** @brief 选择终点（不含/或实现定义，需结合实现使用）。 */
	int SelectionEnd = 0;
	float Boder = 1.0f;
	/** @brief 水平滚动偏移（像素）。 */
	float OffsetX = 0.0f;
	/** @brief 文本与边框之间的内边距（像素）。 */
	float TextMargin = 5.0f;
	/** @brief 创建文本框。 */
	TextBox(std::wstring text, int x, int y, int width = 120, int height = 24);
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
	/** @brief 当前选区长度。 */
	int SelectionLength() const;
	/** @brief 是否存在非空选区。 */
	bool HasSelection() const;
	/** @brief 设置选区，length 为选中字符数。 */
	void Select(int start, int length);
	/** @brief 选中全部文本。 */
	void SelectAll();
	/** @brief 清除选区并将光标放在当前选区末尾。 */
	void ClearSelection();
	/** @brief 清空文本，保留撤销记录。 */
	void Clear();
	/** @brief 在当前光标或选区插入文本。 */
	void InsertText(std::wstring text);
	/** @brief 将选中文本复制到剪贴板。 */
	bool Copy();
	/** @brief 剪切选中文本到剪贴板。 */
	bool Cut();
	/** @brief 从剪贴板粘贴文本。 */
	bool Paste();
	/** @brief 撤销最近一次编辑。 */
	void Undo();
	/** @brief 重做最近一次撤销。 */
	void Redo();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
