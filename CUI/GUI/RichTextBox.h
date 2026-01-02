#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file RichTextBox.h
 * @brief RichTextBox：富文本/大文本输入控件（支持虚拟化渲染）。
 *
 * 设计要点：
 * - 内部维护 buffer，与 Control::Text 在需要时同步
 * - 支持多行、选择区间、滚动条与光标命中测试
 * - 可启用虚拟化：按块（BlockCharCount）构建多个 DWrite TextLayout，以降低超长文本开销
 */
class RichTextBox : public Control
{
private:
	std::wstring buffer;
	bool bufferSyncedFromControl = false;
	::Font* _lastLayoutFont = NULL;

	POINT selectedPos = { 0,0 };
	bool isDraggingScroll = false;
	float _scrollThumbGrabOffsetY = 0.0f;
	IDWriteTextLayout* layOutCache = NULL;
	std::vector<DWRITE_HIT_TEST_METRICS> selRange;
	bool selRangeDirty = true;
	SIZE lastLayoutSize = { 0,0 };

	struct TextBlock
	{
		size_t start = 0;
		size_t len = 0;
		IDWriteTextLayout* layout = NULL;
		float height = -1.0f;
	};
	std::vector<TextBlock> blocks;
	std::vector<float> blockTops; 	bool blocksDirty = true;
	bool blockMetricsDirty = true;
	bool virtualMode = false;
	bool layoutWidthHasScrollBar = false;
	float virtualTotalHeight = 0.0f;
	float cachedRenderWidth = 0.0f;
public:
	virtual UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override;
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	/** @brief 当前文本测量尺寸缓存（供渲染/布局使用）。 */
	D2D1_SIZE_F textSize = { 0,0 };
	/** @brief 鼠标悬停时背景色（实现可能会用到）。 */
	D2D1_COLOR_F UnderMouseColor = Colors::White;
	/** @brief 选区背景色。 */
	D2D1_COLOR_F SelectedBackColor = { 0.f , 0.f , 1.f , 0.5f };
	/** @brief 选区前景色。 */
	D2D1_COLOR_F SelectedForeColor = Colors::White;
	/** @brief 获得焦点时高亮色。 */
	D2D1_COLOR_F FocusedColor = Colors::White;
	/** @brief 滚动条背景色。 */
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	/** @brief 滚动条前景色。 */
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	/** @brief 是否允许多行输入。 */
	bool AllowMultiLine = false;
	/** @brief 是否允许输入 Tab 字符。 */
	bool AllowTabInput = false;
	/** @brief 最大文本长度（超出会被截断）。 */
	size_t MaxTextLength = 1000000;
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
	/** @brief 边框宽度（像素）。 */
	float Boder = 1.5f;
	/** @brief 垂直滚动偏移（像素）。 */
	float OffsetY = 0.0f;
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
	void TrimToMaxLength();
	void RebuildBlocks();
	void ReleaseBlocks();
	void EnsureBlockLayout(int idx, float renderWidth, float renderHeight);
	void EnsureAllBlockMetrics(float renderWidth, float renderHeight);
	int HitTestGlobalIndex(float x, float y);
	bool GetCaretMetrics(int caretIndex, float& outX, float& outY, float& outH);
	void DrawScroll();
	void UpdateScrollDrag(float posY);
	void SetScrollByPos(float yof);
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void UpdateScroll(bool arrival = false);
	void UpdateLayout();
	void UpdateSelRange();
public:
	/** @brief 追加文本（不自动换行）。 */
	void AppendText(std::wstring str);
	/** @brief 追加一行文本（通常会追加换行）。 */
	void AppendLine(std::wstring str);
	/** @brief 获取当前选择文本。 */
	std::wstring GetSelectedString();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
	/** @brief 滚动到末尾。 */
	void ScrollToEnd();
};