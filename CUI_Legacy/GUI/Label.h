#pragma once

/**
 * @file Label.h
 * @brief Label：文本显示控件（Legacy）。
 */
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
/** @brief 文本显示控件。 */
class Label : public Control
{
public:
	float last_width = 0.0f;
	virtual UIClass Type();
	Label(std::wstring text, int x, int y);
	SIZE ActualSize() override;
	void Update() override;
};