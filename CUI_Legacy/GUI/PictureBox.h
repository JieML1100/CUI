#pragma once

/**
 * @file PictureBox.h
 * @brief PictureBox：图片显示控件（Legacy）。
 */
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
class PictureBox : public Control
{
public:
	virtual UIClass Type();
	float Boder = 1.0f;
	PictureBox(int x, int y, int width = 120, int height = 24);
	void Update() override;
};