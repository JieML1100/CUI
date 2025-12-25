#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
class ComboBox : public Control
{
#define COMBO_MIN_SCROLL_BLOCK 16
private:
	int _underMouseIndex = -1;
	bool isDraggingScroll = false;
	float _scrollThumbGrabOffsetY = 0.0f;
	void UpdateScrollDrag(float posY);
public:
	virtual UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override;
	D2D1_COLOR_F UnderMouseBackColor = Colors::SkyBlue;
	D2D1_COLOR_F UnderMouseForeColor = Colors::White;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	D2D1_COLOR_F ButtonBackColor = Colors::SkyBlue;
	SelectionChangedEvent OnSelectionChanged;
	int ExpandCount = 4;
	int ExpandScroll = 0;
	bool Expand = false;
	int SelectedIndex = 0;
	List<std::wstring> values;
	float Boder = 1.5f;
	ComboBox(std::wstring text, int x, int y, int width = 120, int height = 24);
	SIZE ActualSize() override;
	void DrawScroll();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
