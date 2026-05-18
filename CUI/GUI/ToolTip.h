#pragma once
#include "Control.h"

/* Bound hover tooltip component. Add it to the root Form and call Bind(). */
class ToolTip : public Control
{
private:
	class Control* _target = nullptr;
	bool _popupVisible = false;
	float _popupProgress = 0.0f;
	float _popupStartProgress = 0.0f;
	float _popupTargetProgress = 0.0f;
	ULONGLONG _popupAnimStartTick = 0;
	bool _popupAnimating = false;
	POINT CalcPopupOrigin();
	float CurrentPopupProgress();
	void BeginPopupReveal(float startProgress = 0.12f);

public:
	float Border = 1.0f;
	float PaddingX = 10.0f;
	float PaddingY = 6.0f;
	int OffsetX = 10;
	int OffsetY = 8;
	float CornerRadius = 8.0f;
	float ShadowOffsetY = 3.0f;
	UINT PopupAnimationDurationMs = 90;

	D2D1_COLOR_F PopupBackColor = D2D1_COLOR_F{ 0.10f, 0.10f, 0.10f, 0.94f };
	D2D1_COLOR_F PopupBorderColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.18f };
	D2D1_COLOR_F PopupTextColor = Colors::WhiteSmoke;
	D2D1_COLOR_F PopupShadowColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.18f };

	ToolTip(std::wstring text = L"");
	virtual UIClass Type() override;
	bool HitTestChildren() const override { return false; }
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	SIZE ActualSize() override;
	void Update() override;

	void Bind(class Control* target);
	void Bind(class Control* target, const std::wstring& text);
	void Show();
	void Hide();
};
