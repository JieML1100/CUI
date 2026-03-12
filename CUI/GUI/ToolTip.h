#pragma once
#include "Control.h"

/* Bound hover tooltip component. Add it to the root Form and call Bind(). */
class ToolTip : public Control
{
private:
	class Control* _target = nullptr;
	bool _popupVisible = false;
	POINT CalcPopupOrigin();

public:
	float Border = 1.0f;
	float PaddingX = 10.0f;
	float PaddingY = 6.0f;
	int OffsetX = 10;
	int OffsetY = 8;

	D2D1_COLOR_F PopupBackColor = D2D1_COLOR_F{ 0.10f, 0.10f, 0.10f, 0.5f };
	D2D1_COLOR_F PopupBorderColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.18f };
	D2D1_COLOR_F PopupTextColor = Colors::WhiteSmoke;

	ToolTip(std::wstring text = L"");
	virtual UIClass Type() override;
	bool HitTestChildren() const override { return false; }
	SIZE ActualSize() override;
	void Update() override;

	void Bind(class Control* target);
	void Bind(class Control* target, const std::wstring& text);
	void Show();
	void Hide();
};