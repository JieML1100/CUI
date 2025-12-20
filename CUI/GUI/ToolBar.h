#pragma once
#include "Panel.h"
#include "Button.h"

class ToolBar : public Panel
{
public:
	virtual UIClass Type() override;

	int Padding = 6;
	int Gap = 6;
	int ItemHeight = 26;

	ToolBar(int x, int y, int width, int height = 34);

	Button* AddToolButton(std::wstring text, int width = 90);
	Button* AddToolButton(Button* button);
	void LayoutItems();
	void Update() override;
};

