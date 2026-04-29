#pragma once

#include "Control.h"

class ProgressRing : public Control
{
private:
	float _maxValue = 1.0f;
	float _currentValue = 0.5f;
	bool _showPercentage = true;

public:
	virtual UIClass Type();
	float Boder = 0.0f;
	PROPERTY(float, MaxValue);
	GET(float, MaxValue);
	SET(float, MaxValue);

	PROPERTY(float, Value);
	GET(float, Value);
	SET(float, Value);

	PROPERTY(float, PercentageValue);
	GET(float, PercentageValue);
	SET(float, PercentageValue);

	PROPERTY(bool, ShowPercentage);
	GET(bool, ShowPercentage);
	SET(bool, ShowPercentage);

	EventHandler OnValueChanged = EventHandler();
	ProgressRing(int x, int y, int width = 72, int height = 72);
	void SetRange(float maxValue, float value = 0.0f);
	void Increment(float delta);
	void Reset();
	void Update() override;
};
