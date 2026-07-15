#pragma once

#include "../CUI/include/WebBrowser.h"

/**
 * @brief FakeWebBrowser：仅用于设计器的占位控件。
 *
 * 设计时不创建任何真实浏览器/原生窗口，仅渲染一个黑色矩形；继承真实
 * WebBrowser 契约以复用同一份属性元数据与持久化语义。
 */
class FakeWebBrowser : public WebBrowser
{
public:
	FakeWebBrowser(int x, int y, int width, int height);
	UIClass Type() override { return UIClass::UI_WebBrowser; }
	void Update() override;
};
