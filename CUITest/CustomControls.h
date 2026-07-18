#pragma once

/**
 * @file CustomControls.h
 * @brief CUITest 自定义控件声明（用于示例/测试）。
 */
#include "../CUI/include/TextBox.h"
#include "../CUI/include/Label.h"
class CustomTextBox1 : public TextBox
{
public:
	CustomTextBox1(std::wstring text, int x, int y, int width = 120, int height = 24);
};
class CustomLabel1 : public Label
{
public:
	CustomLabel1(std::wstring text, int x, int y);
};
