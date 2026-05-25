#pragma once
#include "Panel.h"

class ScrollView : public Panel
{
public:
	struct ScrollLayout
	{
		bool HasVerticalScroll = false;
		bool HasHorizontalScroll = false;
		float ScrollBarThickness = 8.0f;
		float ViewportWidth = 0.0f;
		float ViewportHeight = 0.0f;
		float ContentWidth = 0.0f;
		float ContentHeight = 0.0f;
		float MaxScrollX = 0.0f;
		float MaxScrollY = 0.0f;
	};

	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	float BorderThickness = 1.5f;
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
	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool ShouldHitTestChildrenAt(int localX, int localY) const override;
	POINT GetChildrenRenderOffset() const override;
	bool ClipsChildren() override { return true; }
	D2D1_RECT_F GetChildrenClipRect() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

	void ScrollBy(int deltaX, int deltaY);
	void SetScrollOffset(int offsetX, int offsetY);

private:
	bool _draggingVerticalScrollBar = false;
	bool _draggingHorizontalScrollBar = false;
	float _verticalScrollThumbGrabOffset = 0.0f;
	float _horizontalScrollThumbGrabOffset = 0.0f;

	void PerformScrollContentLayout();
	ScrollLayout CalcScrollLayout();
	SIZE MeasureContentSize();
	void ClampScrollOffsets(const ScrollLayout& layout);
	void DrawScrollBars(const ScrollLayout& layout);
	void UpdateVerticalScrollByThumb(float localY, const ScrollLayout& layout);
	void UpdateHorizontalScrollByThumb(float localX, const ScrollLayout& layout);
	bool HitChild(Control* child, int localX, int localY, int& childX, int& childY) const;
	bool HitVerticalScrollBar(int localX, int localY, const ScrollLayout& layout) const;
	bool HitHorizontalScrollBar(int localX, int localY, const ScrollLayout& layout) const;
};
