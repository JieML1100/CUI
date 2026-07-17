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
	D2D1_COLOR_F LineColor = cui::theme::palette::BorderStrong;

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
protected:
	void PerformPendingLayout() override;

private:
	static constexpr int AutoItemHeight = -2;
	std::unordered_map<Control*, SIZE> _toolItemSizeOverrides;
	SIZE GetToolItemLayoutSize(Control* item);
	void ApplyItemHeightToAutoItems(int value);

	int _horizontalPadding = 6;
	int _gap = 6;
	int _itemHeight = 26;
	float _itemCornerRatio = 0.28f;
	D2D1_COLOR_F _separatorColor =
		cui::theme::palette::Border;
	D2D1_COLOR_F _bottomLineColor =
		cui::theme::palette::Border;
	bool _showBottomLine = true;

public:
	/** Sentinel used by item-size overrides that follow ToolBar::ItemHeight. */
	static constexpr int AutoItemHeightOverride = -2;

	virtual UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;

#define CUI_TOOL_BAR_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	/** @brief 工具栏内容的水平内缩（DIP）。 */
	CUI_TOOL_BAR_PROPERTY(int, HorizontalPadding);
	/** @brief 工具项之间的间距（DIP）。 */
	CUI_TOOL_BAR_PROPERTY(int, Gap);
	/** @brief 自动高度工具项的默认高度（DIP）。 */
	CUI_TOOL_BAR_PROPERTY(int, ItemHeight);
	/** @brief 工具按钮默认圆角比例（乘以按钮高度）。 */
	CUI_TOOL_BAR_PROPERTY(float, ItemCornerRatio);
	/** @brief 工具栏分隔线颜色。 */
	CUI_TOOL_BAR_PROPERTY(D2D1_COLOR_F, SeparatorColor);
	/** @brief 工具栏底部分隔线颜色。 */
	CUI_TOOL_BAR_PROPERTY(D2D1_COLOR_F, BottomLineColor);
	/** @brief 是否绘制底部分隔线。 */
	CUI_TOOL_BAR_PROPERTY(bool, ShowBottomLine);

#undef CUI_TOOL_BAR_PROPERTY

	/** @brief 创建工具栏。 */
	ToolBar(int x, int y, int width, int height = 34);

	/** @brief 添加已有控件实例到工具栏，ToolBar 负责位置排布但不接管额外样式。 */
	Control* AddToolItem(Control* item, int width = -1, int height = -1);
	/** @brief 查询工具项的布局尺寸覆盖，供所有权转移保真使用。 */
	bool TryGetToolItemSizeOverride(
		Control* item, SIZE& value) const noexcept;
	/** @brief 恢复已挂载工具项的布局尺寸覆盖。 */
	void SetToolItemSizeOverride(Control* item, SIZE value);
	/** @brief 移除已分离工具项的布局尺寸覆盖。 */
	void ClearToolItemSizeOverride(Control* item) noexcept;

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

	template<typename T>
	T* AddOwned(std::unique_ptr<T> item, int width = -1, int height = -1)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		if (!item)
			throw std::invalid_argument("不能添加空控件");
		T* raw = item.get();
		AddToolItem(raw, width, height);
		item.release();
		return raw;
	}

	template<typename T, typename... Args>
	T* Add(Args&&... args)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		return AddOwned(std::make_unique<T>(std::forward<Args>(args)...));
	}

	/** @brief 创建并添加文字按钮。 */
	Button* AddTextButton(std::wstring text, int width = 90);
	/** Creates a detached button with the same defaults as AddToolButton(). */
	std::unique_ptr<Button> CreateToolButton(
		std::wstring text, int width = 90) const;
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
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
