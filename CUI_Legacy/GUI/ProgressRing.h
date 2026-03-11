#pragma once

#include "Control.h"

class ProgressRing : public Control
{
private:
	float _percentageValue = 0.5f;
	bool _showPercentage = true;

public:
	virtual UIClass Type();
	float Boder = 0.0f;

	PROPERTY(float, PercentageValue);
	GET(float, PercentageValue);
	SET(float, PercentageValue);

	PROPERTY(bool, ShowPercentage);
	GET(bool, ShowPercentage);
	SET(bool, ShowPercentage);

	ProgressRing(int x, int y, int width = 72, int height = 72);
	void Update() override;
};