#pragma once
#include "Panel.h"
#include "Button.h"
#include "ComboBox.h"
#include "CheckBox.h"
#include <unordered_map>

/**
 * @file ToolBar.h
 * @brief ToolBar：通用工具栏容器。
 *
 * 约定：
 * - 通过 AddToolItem 添加任意控件项
 * - 通过 AddTextButton/AddIconButton/AddToolComboBox/AddToolCheckBox 快速创建常见工具项
 * - 旧的 AddToolButton API 保留，兼容已有代码
 * - LayoutItems 负责对内部工具项进行水平排布
 */

class ToolBarSeparator : public Control
{
public:
	D2D1_COLOR_F LineColor = D2D1_COLOR_F{ 1,1,1,0.28f };

	virtual UIClass Type() override;
	ToolBarSeparator(int width = 1, int height = 20);
	void Update() override;
};

class ToolBarSpacer : public Control
{
public:
	virtual UIClass Type() override;
	ToolBarSpacer(int width = 8);
	void Update() override;
};

class ToolBar : public Panel
{
private:
	std::unordered_map<Control*, SIZE> _toolItemSizeOverrides;
	SIZE GetToolItemLayoutSize(Control* item);

public:
	virtual UIClass Type() override;

	/** @brief 工具栏内边距（像素）。 */
	int Padding = 6;
	/** @brief 工具项之间的间距（像素）。 */
	int Gap = 6;
	/** @brief 工具项默认高度（像素）。 */
	int ItemHeight = 26;
	/** @brief 工具栏背景圆角半径。 */
	float CornerRadius = 8.0f;
	/** @brief 工具按钮默认圆角比例（乘以按钮高度）。 */
	float ItemCornerRatio = 0.28f;
	/** @brief 工具栏分隔线颜色。 */
	D2D1_COLOR_F SeparatorColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.18f };
	/** @brief 工具栏底部分隔线颜色。 */
	D2D1_COLOR_F BottomLineColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.14f };
	/** @brief 是否绘制底部分隔线。 */
	bool ShowBottomLine = true;

	/** @brief 创建工具栏。 */
	ToolBar(int x, int y, int width, int height = 34);

	/** @brief 添加已有控件实例到工具栏，ToolBar 负责位置排布但不接管额外样式。 */
	Control* AddToolItem(Control* item, int width = -1, int height = -1);

	template<typename T>
	T AddToolItem(T item, int width = -1, int height = -1)
	{
		return (T)AddToolItem((Control*)item, width, height);
	}

	template<typename T>
	T AddControl(T c)
	{
		return AddToolItem(c);
	}

	/** @brief 创建并添加文字按钮。 */
	Button* AddTextButton(std::wstring text, int width = 90);
	/** @brief 创建并添加一个工具按钮（自动 new）。返回按钮指针以便绑定事件。 */
	Button* AddToolButton(std::wstring text, int width = 90);
	/** @brief 添加已有按钮实例到工具栏（将其作为子控件）。 */
	Button* AddToolButton(Button* button);
	/** @brief 创建并添加图标按钮。 */
	Button* AddIconButton(std::shared_ptr<BitmapSource> image, int width = -1, std::wstring text = L"");
	/** @brief 创建并添加下拉框。 */
	ComboBox* AddToolComboBox(std::wstring text, int width = 120);
	/** @brief 创建并添加复选框。 */
	CheckBox* AddToolCheckBox(std::wstring text, int width = -1);
	/** @brief 添加一条竖向分隔线。 */
	ToolBarSeparator* AddSeparator(int width = 1);
	/** @brief 添加空白间距。 */
	ToolBarSpacer* AddSpacer(int width = 8);
	/** @brief 重新布局所有工具项。 */
	void LayoutItems();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
