#pragma once

/**
 * @file ProgressBar.h
 * @brief ProgressBar：进度条控件（Legacy）。
 */
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
class ProgressBar : public Control
{
private:
	float _percentageValue = 0.5f;
public:
	virtual UIClass Type();
	float Boder = 1.5f;

	PROPERTY(float, PercentageValue);
	GET(float, PercentageValue);
	SET(float, PercentageValue);
	ProgressBar(int x, int y, int width = 120, int height = 24);
	void Update() override;
};