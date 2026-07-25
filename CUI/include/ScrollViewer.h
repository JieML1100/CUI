#pragma once
#include "ContentControl.h"

enum class ScrollBarVisibility : int
{
	Disabled = 0,
	Auto = 1,
	Hidden = 2,
	Visible = 3,
};

class ScrollViewer : public ContentControl
{
private:
	D2D1_COLOR_F _scrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F _scrollForeColor = cui::theme::palette::ScrollThumb;
	float _scrollBarThickness = 8.0f;
	ScrollBarVisibility _horizontalScrollBarVisibility =
		ScrollBarVisibility::Auto;
	ScrollBarVisibility _verticalScrollBarVisibility =
		ScrollBarVisibility::Auto;
	double _extentWidth = 0.0;
	double _extentHeight = 0.0;
	double _viewportWidth = 0.0;
	double _viewportHeight = 0.0;
	double _horizontalOffset = 0.0;
	double _verticalOffset = 0.0;
	int _mouseWheelStep = 48;

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Pane, L"ScrollViewer");
	}
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
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

	ScrollViewer();

	UIClass Type() override;
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }

	using UIElement::OnScrollChanged;

	PROPERTY(ScrollBarVisibility, HorizontalScrollBarVisibility);
	GET(ScrollBarVisibility, HorizontalScrollBarVisibility);
	SET(ScrollBarVisibility, HorizontalScrollBarVisibility);
	PROPERTY(ScrollBarVisibility, VerticalScrollBarVisibility);
	GET(ScrollBarVisibility, VerticalScrollBarVisibility);
	SET(ScrollBarVisibility, VerticalScrollBarVisibility);

	READONLY_PROPERTY(double, ExtentWidth);
	GET(double, ExtentWidth);
	READONLY_PROPERTY(double, ExtentHeight);
	GET(double, ExtentHeight);
	READONLY_PROPERTY(double, ViewportWidth);
	GET(double, ViewportWidth);
	READONLY_PROPERTY(double, ViewportHeight);
	GET(double, ViewportHeight);
	READONLY_PROPERTY(double, HorizontalOffset);
	GET(double, HorizontalOffset);
	READONLY_PROPERTY(double, VerticalOffset);
	GET(double, VerticalOffset);

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(Key key) const override;
	bool ShouldHitTestChildrenAt(int localX, int localY) const override;
	cui::core::Point GetVisualChildrenRenderOffset() const override;
	bool ClipsChildren() override { return true; }
	D2D1_RECT_F GetVisualChildrenClipRect() override;
protected:
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
public:
	void LineLeft();
	void LineRight();
	void LineUp();
	void LineDown();
	void PageLeft();
	void PageRight();
	void PageUp();
	void PageDown();
	void ScrollToHorizontalOffset(double offset);
	void ScrollToVerticalOffset(double offset);
	void ScrollToHome();
	void ScrollToEnd();
	/** Scrolls the nearest descendant rectangle into the current viewport. */
	bool BringDescendantIntoView(Control* descendant);

private:
	bool _draggingVerticalScrollBar = false;
	bool _draggingHorizontalScrollBar = false;
	float _verticalScrollThumbGrabOffset = 0.0f;
	float _horizontalScrollThumbGrabOffset = 0.0f;

	void PerformScrollContentLayout();
	ScrollLayout CalcScrollLayout();
	cui::core::Size MeasureContentSizeDip();
	void ClampScrollOffsets(const ScrollLayout& layout);
	void SetScrollOffsetCore(double horizontalOffset, double verticalOffset);
	void PublishScrollState(
		const ScrollLayout& layout,
		double horizontalOffset,
		double verticalOffset);
	void DrawScrollBars(const ScrollLayout& layout);
	void UpdateVerticalScrollByThumb(float localY, const ScrollLayout& layout);
	void UpdateHorizontalScrollByThumb(float localX, const ScrollLayout& layout);
	bool HitChild(Control* child, int localX, int localY, int& childX, int& childY) const;
	bool HitVerticalScrollBar(int localX, int localY, const ScrollLayout& layout) const;
	bool HitHorizontalScrollBar(int localX, int localY, const ScrollLayout& layout) const;
};
