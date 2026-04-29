#pragma once
#include "Panel.h"

class ScrollView : public Panel
{
public:
	struct ScrollLayout
	{
		bool NeedV = false;
		bool NeedH = false;
		float ScrollBarSize = 8.0f;
		float ViewportWidth = 0.0f;
		float ViewportHeight = 0.0f;
		float ContentWidth = 0.0f;
		float ContentHeight = 0.0f;
		float MaxScrollX = 0.0f;
		float MaxScrollY = 0.0f;
	};

	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	float Boder = 1.5f;
	bool AlwaysShowVScroll = false;
	bool AlwaysShowHScroll = false;
	bool AutoContentSize = true;
	SIZE ContentSize = SIZE{ 0, 0 };
	int ScrollXOffset = 0;
	int ScrollYOffset = 0;
	int MouseWheelStep = 48;

	ScrollView();
	ScrollView(int x, int y, int width, int height);

	UIClass Type() override;
	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool ShouldHitTestChildrenAt(int xof, int yof) const override;
	POINT GetChildrenRenderOffset() const override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	void ScrollBy(int dx, int dy);
	void SetScrollOffset(int x, int y);
	/** @brief 返回当前滚动布局与范围信息。 */
	ScrollLayout GetScrollLayout();
	/** @brief 当前水平最大滚动偏移。 */
	int MaxScrollX();
	/** @brief 当前垂直最大滚动偏移。 */
	int MaxScrollY();
	/** @brief 滚动到左上角。 */
	void ScrollToStart();
	/** @brief 滚动到底部/右侧末尾。 */
	void ScrollToEnd();
	/** @brief 滚动到顶部。 */
	void ScrollToTop();
	/** @brief 滚动到底部。 */
	void ScrollToBottom();
	/** @brief 滚动到最左侧。 */
	void ScrollToLeft();
	/** @brief 滚动到最右侧。 */
	void ScrollToRight();
	/** @brief 让指定子控件尽量进入可视区域。 */
	bool ScrollIntoView(Control* child, int margin = 0);

private:
	bool _dragVScroll = false;
	bool _dragHScroll = false;
	float _vScrollThumbGrabOffset = 0.0f;
	float _hScrollThumbGrabOffset = 0.0f;

	void PerformScrollContentLayout();
	ScrollLayout CalcScrollLayout();
	SIZE MeasureContentSize();
	void ClampScrollOffsets(const ScrollLayout& layout);
	void DrawScrollBars(const ScrollLayout& layout);
	void UpdateScrollByThumbY(float localY, const ScrollLayout& layout);
	void UpdateScrollByThumbX(float localX, const ScrollLayout& layout);
	bool HitChild(Control* child, int xof, int yof, int& childX, int& childY) const;
	bool HitVScrollBar(int xof, int yof, const ScrollLayout& layout) const;
	bool HitHScrollBar(int xof, int yof, const ScrollLayout& layout) const;
};
