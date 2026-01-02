#pragma once

/**
 * @file Switch.h
 * @brief Switch：开关控件（Legacy）。
 */
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
class Switch : public Control
{
	float last_width = 0.0f;
public:
	virtual UIClass Type();
	D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
	float Boder = 1.5f;
	Switch(int x = 0, int y = 0, int width = 60, int height = 22);
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};