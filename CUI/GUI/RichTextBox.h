#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
class RichTextBox : public Control
{
private:
	// NOTE: Control::Text 的 getter 会按值返回，超大文本下频繁访问会导致整串复制，极其影响性能。
	// RichTextBox 内部统一使用 buffer，必要时再同步到 Control::_text。
	std::wstring buffer;
	bool bufferSyncedFromControl = false;

	POINT selectedPos = {0,0};
	bool isDraggingScroll = false;
	IDWriteTextLayout* layOutCache = NULL;
	std::vector<DWRITE_HIT_TEST_METRICS> selRange;
	bool selRangeDirty = true;
	SIZE lastLayoutSize = { 0,0 };

	// 超大文本虚拟化：按块（chunk）拆分为多个 layout，仅绘制可视块
	struct TextBlock
	{
		size_t start = 0;
		size_t len = 0;
		IDWriteTextLayout* layout = NULL;
		float height = -1.0f;
	};
	std::vector<TextBlock> blocks;
	std::vector<float> blockTops; // prefix-top cache: blockTops[i] = y top of block i
	bool blocksDirty = true;
	bool blockMetricsDirty = true;
	bool virtualMode = false;
	bool layoutWidthHasScrollBar = false;
	float virtualTotalHeight = 0.0f;
	float cachedRenderWidth = 0.0f;
public:
	virtual UIClass Type();
	// 光标闪烁：仅“无选区时”需要周期刷新；大文本虚拟化下也需要驱动 caret 绘制
	int DesiredFrameIntervalMs() override { return (this->IsSelected() && this->SelectionStart == this->SelectionEnd) ? 100 : 0; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	D2D1_SIZE_F textSize = { 0,0 };
	D2D1_COLOR_F UnderMouseColor = Colors::White;
	D2D1_COLOR_F SelectedBackColor = { 0.f , 0.f , 1.f , 0.5f };
	D2D1_COLOR_F SelectedForeColor = Colors::White;
	D2D1_COLOR_F FocusedColor = Colors::White;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	bool AllowMultiLine = false;
	// 超大文本场景强烈建议设置上限，避免 IDWriteTextLayout/HitTest 在百万级文本上出现明显卡顿。
	// 0 表示不限制（不推荐）。默认 1,000,000 字符。
	size_t MaxTextLength = 1000000;
	// 开启虚拟化后，超出阈值会切换到“按块渲染”模式，极大提升大文本下的输入/滚动性能。
	bool EnableVirtualization = true;
	size_t VirtualizeThreshold = 20000;
	size_t BlockCharCount = 4096;
	int SelectionStart = 0;
	int SelectionEnd = 0;
	float Boder = 1.5f;
	float OffsetY = 0.0f;
	float TextMargin = 5.0f;
	RichTextBox(std::wstring text, int x, int y, int width = 120, int height = 24);
private:
	// 光标区域缓存：用于 WM_TIMER 局部无效化
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
	void AppendText(std::wstring str);
	void AppendLine(std::wstring str);
	std::wstring GetSelectedString();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
	void ScrollToEnd();
};