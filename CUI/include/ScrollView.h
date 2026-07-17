#pragma once
#include "Panel.h"

class ScrollView : public Panel
{
private:
	D2D1_COLOR_F _scrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F _scrollForeColor = cui::theme::palette::ScrollThumb;
	float _scrollBarThickness = 8.0f;
	bool _alwaysShowVScroll = false;
	bool _alwaysShowHScroll = false;
	bool _autoContentSize = true;
	SIZE _contentSize = SIZE{ 0, 0 };
	int _mouseWheelStep = 48;

protected:
	void PerformPendingLayout() override;

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

	int ScrollXOffset = 0;
	int ScrollYOffset = 0;

	ScrollView();
	ScrollView(int x, int y, int width, int height);

	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;

#define CUI_SCROLL_VIEW_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_SCROLL_VIEW_PROPERTY(D2D1_COLOR_F, ScrollBackColor);
	CUI_SCROLL_VIEW_PROPERTY(D2D1_COLOR_F, ScrollForeColor);
	CUI_SCROLL_VIEW_PROPERTY(float, ScrollBarThickness);
	CUI_SCROLL_VIEW_PROPERTY(bool, AlwaysShowVScroll);
	CUI_SCROLL_VIEW_PROPERTY(bool, AlwaysShowHScroll);
	CUI_SCROLL_VIEW_PROPERTY(bool, AutoContentSize);
	CUI_SCROLL_VIEW_PROPERTY(SIZE, ContentSize);
	CUI_SCROLL_VIEW_PROPERTY(int, MouseWheelStep);

#undef CUI_SCROLL_VIEW_PROPERTY

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
	cui::core::Size MeasureContentSizeDip();
	void ClampScrollOffsets(const ScrollLayout& layout);
	void DrawScrollBars(const ScrollLayout& layout);
	void UpdateVerticalScrollByThumb(float localY, const ScrollLayout& layout);
	void UpdateHorizontalScrollByThumb(float localX, const ScrollLayout& layout);
	bool HitChild(Control* child, int localX, int localY, int& childX, int& childY) const;
	bool HitVerticalScrollBar(int localX, int localY, const ScrollLayout& layout) const;
	bool HitHorizontalScrollBar(int localX, int localY, const ScrollLayout& layout) const;
};
