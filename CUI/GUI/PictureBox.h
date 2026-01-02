#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file PictureBox.h
 * @brief PictureBox：图片显示控件。
 *
 * 说明：
 * - 通常配合基类 Control::Image 与 Control::SizeMode 使用
 * - Update 中负责绘制边框与图片内容
 */
class PictureBox : public Control
{
public:
	virtual UIClass Type();
	/** @brief 边框宽度（像素）。 */
	float Boder = 1.0f;
	/** @brief 创建图片控件。 */
	PictureBox(int x, int y, int width = 120, int height = 24);
	void Update() override;
};