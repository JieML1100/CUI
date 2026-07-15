#pragma once
#include "Panel.h"

/**
 * @file SplitContainer.h
 * @brief SplitContainer: two panels separated by a draggable splitter.
 */
class SplitContainer : public Panel
{
protected:
	void PerformPendingLayout() override;

public:
	::Orientation SplitOrientation = Orientation::Horizontal;
	int SplitterDistance = 160;
	int SplitterWidth = 6;
	int Panel1MinSize = 48;
	int Panel2MinSize = 48;
	bool IsSplitterFixed = false;
	D2D1_COLOR_F SplitterColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.08f };
	D2D1_COLOR_F SplitterHotColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.14f };
	D2D1_COLOR_F SplitterPressedColor = D2D1_COLOR_F{ 0.35f, 0.64f, 0.96f, 0.90f };
	float SplitterCornerRadius = 3.0f;
	float SplitterVisualInset = 8.0f;

	SplitContainer();
	SplitContainer(int x, int y, int width, int height);

	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	CursorKind QueryCursor(int localX, int localY) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

	void SetSplitOrientation(::Orientation value);
	::Orientation GetSplitOrientation() const { return SplitOrientation; }
	void SetSplitterDistance(int value);
	int GetSplitterDistance() const { return SplitterDistance; }
	void SetSplitterWidth(int value);
	int GetSplitterWidth() const { return SplitterWidth; }
	void SetPanel1MinSize(int value);
	int GetPanel1MinSize() const { return Panel1MinSize; }
	void SetPanel2MinSize(int value);
	int GetPanel2MinSize() const { return Panel2MinSize; }
	void SetIsSplitterFixed(bool value);
	bool GetIsSplitterFixed() const { return IsSplitterFixed; }
	void SetSplitterColor(D2D1_COLOR_F value);
	D2D1_COLOR_F GetSplitterColor() const { return SplitterColor; }
	void SetSplitterHotColor(D2D1_COLOR_F value);
	D2D1_COLOR_F GetSplitterHotColor() const { return SplitterHotColor; }
	void SetSplitterPressedColor(D2D1_COLOR_F value);
	D2D1_COLOR_F GetSplitterPressedColor() const { return SplitterPressedColor; }
	void SetSplitterCornerRadius(float value);
	float GetSplitterCornerRadius() const { return SplitterCornerRadius; }
	void SetSplitterVisualInset(float value);
	float GetSplitterVisualInset() const { return SplitterVisualInset; }
	void RefreshSplitterLayout();

	Panel* FirstPanel() const { return _panel1; }
	Panel* SecondPanel() const { return _panel2; }

private:
	Panel* _panel1 = nullptr;
	Panel* _panel2 = nullptr;
	bool _draggingSplitter = false;
	int _splitterDragOffset = 0;
	bool _isSplitterHovered = false;

	void EnsureChildPanels();
	void ArrangeSplitPanels();
	RECT GetSplitterRect();
	bool HitSplitter(int localX, int localY);
	int ClampSplitterDistance(int value);
	void SetSplitterDistanceInternal(int value);
	bool HitChildPanel(Panel* child, int localX, int localY, int& childX, int& childY);
};
