#include "CustomControls.h"

CustomTextBox1::CustomTextBox1(
	std::wstring text,
	int x,
	int y,
	int width,
	int height)
	: TextBox(std::move(text), x, y, width, height)
{
}

CustomLabel1::CustomLabel1(std::wstring text, int x, int y)
	: Label(std::move(text), x, y)
{
}
