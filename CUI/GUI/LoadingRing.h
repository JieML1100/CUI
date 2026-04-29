#pragma once

#include "Control.h"

class LoadingRing : public Control
{
private:
	bool _active = true;
	ULONGLONG _animStartTick = 0;
	UINT _animationPeriodMs = 900;

	float GetAnimationPhase() const;

public:
	virtual UIClass Type();
	float Boder = 0.0f;

	PROPERTY(bool, Active);
	GET(bool, Active);
	SET(bool, Active);

	LoadingRing(int x, int y, int width = 48, int height = 48);
	void Start();
	void Stop();
	void Restart();
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
};
