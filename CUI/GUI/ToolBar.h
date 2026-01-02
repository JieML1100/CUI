#pragma once
#include "Panel.h"
#include "Button.h"

/**
 * @file ToolBar.h
 * @brief ToolBar：工具栏容器。
 *
 * 约定：
 * - 通过 AddToolButton 添加按钮项
 * - LayoutItems 负责对内部按钮进行水平排布
 */

class ToolBar : public Panel
{
public:
	virtual UIClass Type() override;

	/** @brief 工具栏内边距（像素）。 */
	int Padding = 6;
	/** @brief 按钮之间的间距（像素）。 */
	int Gap = 6;
	/** @brief 工具项高度（像素）。 */
	int ItemHeight = 26;

	/** @brief 创建工具栏。 */
	ToolBar(int x, int y, int width, int height = 34);

	/** @brief 创建并添加一个工具按钮（自动 new）。返回按钮指针以便绑定事件。 */
	Button* AddToolButton(std::wstring text, int width = 90);
	/** @brief 添加已有按钮实例到工具栏（将其作为子控件）。 */
	Button* AddToolButton(Button* button);
	/** @brief 重新布局所有工具项。 */
	void LayoutItems();
	void Update() override;
};

