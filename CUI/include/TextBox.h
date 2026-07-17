#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file TextBox.h
 * @brief TextBox：单行文本输入控件（支持选择、光标、滚动、IME）。
 *
 * 关键字段：
 * - SelectionStart/SelectionEnd：选择区间（基于字符索引）
 * - HorizontalScrollOffset：水平滚动偏移（像素），用于长文本显示
 * - GetAnimatedInvalidRect：用于光标闪烁等动画区域增量刷新
 */
class TextBox : public Control
{
private:
	D2D1_COLOR_F _underMouseColor = cui::theme::palette::SurfaceSubtle;
	D2D1_COLOR_F _selectedBackColor = cui::theme::palette::SelectionBack;
	D2D1_COLOR_F _selectedForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _focusedColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _disabledOverlayColor = cui::theme::palette::DisabledOverlay;
	float _borderThickness = 1.0f;
	float _cornerRadius = 6.0f;
	float _focusBorder = 1.6f;
	float _textMargin = 5.0f;
public:
	virtual UIClass Type();
	void EnsureBindingPropertiesRegistered() override;
	CursorKind QueryCursor(int localX, int localY) override { (void)localX; (void)localY; return this->Enable ? CursorKind::IBeam : CursorKind::Arrow; }
	bool HandlesMouseWheel() const override { return true; }
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	/** @brief 当前文本测量尺寸缓存（像素）。 */
	D2D1_SIZE_F textSize = { 0,0 };
	PROPERTY(D2D1_COLOR_F, UnderMouseColor);
	GET(D2D1_COLOR_F, UnderMouseColor);
	SET(D2D1_COLOR_F, UnderMouseColor);
	PROPERTY(D2D1_COLOR_F, SelectedBackColor);
	GET(D2D1_COLOR_F, SelectedBackColor);
	SET(D2D1_COLOR_F, SelectedBackColor);
	PROPERTY(D2D1_COLOR_F, SelectedForeColor);
	GET(D2D1_COLOR_F, SelectedForeColor);
	SET(D2D1_COLOR_F, SelectedForeColor);
	PROPERTY(D2D1_COLOR_F, FocusedColor);
	GET(D2D1_COLOR_F, FocusedColor);
	SET(D2D1_COLOR_F, FocusedColor);
	D2D1_COLOR_F ScrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F ScrollForeColor = cui::theme::palette::ScrollThumb;
	PROPERTY(D2D1_COLOR_F, DisabledOverlayColor);
	GET(D2D1_COLOR_F, DisabledOverlayColor);
	SET(D2D1_COLOR_F, DisabledOverlayColor);
	/** @brief 选择起点（含）。 */
	int SelectionStart = 0;
	/** @brief 选择终点（不含/或实现定义，需结合实现使用）。 */
	int SelectionEnd = 0;
	PROPERTY(float, BorderThickness);
	GET(float, BorderThickness);
	SET(float, BorderThickness);
	/** @brief 圆角半径。 */
	PROPERTY(float, CornerRadius);
	GET(float, CornerRadius);
	SET(float, CornerRadius);
	/** @brief 聚焦时边框宽度。 */
	PROPERTY(float, FocusBorder);
	GET(float, FocusBorder);
	SET(float, FocusBorder);
	/** @brief 水平滚动偏移（像素）。 */
	float HorizontalScrollOffset = 0.0f;
	/** @brief 文本与边框之间的内边距（像素）。 */
	PROPERTY(float, TextMargin);
	GET(float, TextMargin);
	SET(float, TextMargin);
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
	void Undo();
	void Redo();
public:
	/** @brief 返回当前选中的文本片段。 */
	std::wstring GetSelectedString();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
