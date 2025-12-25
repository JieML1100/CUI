#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
class RichTextBox : public Control
{
private:
	std::wstring buffer;
	bool bufferSyncedFromControl = false;

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
	D2D1_SIZE_F textSize = { 0,0 };
	D2D1_COLOR_F UnderMouseColor = Colors::White;
	D2D1_COLOR_F SelectedBackColor = { 0.f , 0.f , 1.f , 0.5f };
	D2D1_COLOR_F SelectedForeColor = Colors::White;
	D2D1_COLOR_F FocusedColor = Colors::White;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	bool AllowMultiLine = false;
	bool AllowTabInput = false;
	size_t MaxTextLength = 1000000;
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