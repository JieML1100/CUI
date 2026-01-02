#pragma once
#include "Control.h"
#include "TextBox.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file RoundTextBox.h
 * @brief RoundTextBox：圆角文本框。
 *
 * 继承 TextBox，仅在渲染外观（圆角/边框）上做差异化处理。
 */
class RoundTextBox : public TextBox
{
public:
	/** @brief 创建圆角文本框。 */
	RoundTextBox(std::wstring text, int x, int y, int width = 120, int height = 24);
public:
	void Update() override;
};