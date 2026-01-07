#pragma once

#include "../CUI_Legacy/GUI/Control.h"

/**
 * @brief FakeWebBrowser：仅用于设计器的占位控件。
 *
 * 设计时不创建任何真实浏览器/原生窗口，仅渲染一个黑色矩形。
 */
class FakeWebBrowser : public Control
{
public:
	FakeWebBrowser(int x, int y, int width, int height);
	UIClass Type() override { return UIClass::UI_WebBrowser; }
	void Update() override;
};
