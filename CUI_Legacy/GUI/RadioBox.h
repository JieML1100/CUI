#pragma once

/**
 * @file RadioBox.h
 * @brief RadioBox：单选框风格控件（Legacy）。
 */
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
class RadioBox : public Control
{
	float last_width = 0.0f;
public:
	virtual UIClass Type();
	D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
	float Boder = 1.5f;
	RadioBox(std::wstring text, int x, int y);
	SIZE ActualSize() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};