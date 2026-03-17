#pragma once
#include "Panel.h"

/**
 * @file SplitContainer.h
 * @brief SplitContainer: two panels separated by a draggable splitter.
 */
class SplitContainer : public Panel
{
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

	SplitContainer();
	SplitContainer(int x, int y, int width, int height);

	UIClass Type() override;
	CursorKind QueryCursor(int xof, int yof) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
	void SetSplitterDistance(int value);
	void RefreshSplitterLayout();

	Panel* FirstPanel() const { return _panel1; }
	Panel* SecondPanel() const { return _panel2; }

private:
	Panel* _panel1 = nullptr;
	Panel* _panel2 = nullptr;
	bool _draggingSplitter = false;
	int _dragOffset = 0;
	bool _hoverSplitter = false;

	void EnsureChildPanels();
	void ArrangeSplitPanels();
	RECT GetSplitterRect();
	bool HitSplitter(int xof, int yof);
	int ClampSplitterDistance(int value);
	void SetSplitterDistanceInternal(int value);
	bool HitChildPanel(Panel* child, int xof, int yof, int& childX, int& childY);
};
