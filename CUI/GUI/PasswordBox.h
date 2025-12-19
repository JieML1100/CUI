#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
class PasswordBox : public Control
{
public:
	virtual UIClass Type();
	// 光标闪烁：仅“无选区时”需要周期刷新
	int DesiredFrameIntervalMs() override { return (this->IsSelected() && this->SelectionStart == this->SelectionEnd) ? 100 : 0; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	D2D1_COLOR_F UnderMouseColor = Colors::White;
	D2D1_COLOR_F SelectedBackColor = { 0.f , 0.f , 1.f , 0.5f };
	D2D1_COLOR_F SelectedForeColor = Colors::White;
	D2D1_COLOR_F FocusedColor = Colors::White;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	D2D1_SIZE_F textSize = { 0,0 };
	int SelectionStart = 0;
	int SelectionEnd = 0;
	float Boder = 1.5f;
	float OffsetX = 0.0f;
	float TextMargin = 5.0f;
protected:
	// 光标区域缓存：用于 WM_TIMER 局部无效化
	D2D1_RECT_F _caretRectCache = { 0,0,0,0 };
	bool _caretRectCacheValid = false;
public:
	PasswordBox(std::wstring text, int x, int y, int width = 120, int height = 24);
private:
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void UpdateScroll(bool arrival = false);
public:
	std::wstring GetSelectedString();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};