#pragma once
#include "Panel.h"
#include "Label.h"

/**
 * @file StatusBar.h
 * @brief StatusBar：状态栏控件（面板容器）。
 *
 * 特性：
 * - 支持“左/右”默认文本区，以及任意数量的 Part 分段
 * - 可通过 TopMost 让 Form 以独立通道进行渲染与命中测试（类似 MainMenu 处理方式）
 */

class StatusBar : public Panel
{
private:
	struct Part
	{
		/** @brief 分段对应的 Label 控件。 */
		Label* LabelCtrl = nullptr;
		/** @brief 分段宽度；0 表示自适应（由实现决定）。 */
		int Width = 0;
	};

	std::vector<Part> _parts;
	std::vector<float> _separatorsX;
	std::vector<D2D1_RECT_F> _partRects;

	Label* _leftLabel = nullptr;
	Label* _rightLabel = nullptr;

	int _partInnerPadding = 8;

	void EnsureDefaultParts();
	void UpdateCompatPointers();

public:
	virtual UIClass Type() override;

	// When true, Form will treat this StatusBar like MainMenu: rendered and hit-tested
	// through a dedicated top-most channel (independent from normal control z-order).
	/**
	 * @brief 是否启用顶层通道渲染/命中测试。
	 *
	 * 为 true 时，Form 会像处理 MainMenu 一样处理该 StatusBar，使其不受普通控件 Z 序影响。
	 */
	bool TopMost = true;

	/** @brief 外边距/内边距（像素，具体含义由实现决定）。 */
	int Padding = 6;
	/** @brief 分段之间的间距（像素）。 */
	int Gap = 10;
	/** @brief 状态栏背景圆角半径；停靠底栏默认不使用圆角。 */
	float CornerRadius = 0.0f;
	/** @brief 固定分段胶囊圆角半径。 */
	float PartCornerRadius = 8.0f;
	/** @brief 分隔线颜色。 */
	D2D1_COLOR_F SeparatorColor = D2D1_COLOR_F{ 1, 1, 1, 0.12f };
	/** @brief 顶部细线颜色。 */
	D2D1_COLOR_F TopLineColor = D2D1_COLOR_F{ 1, 1, 1, 0.12f };
	/** @brief 固定宽度/自适应分段背景色，UsePartPills=true 时生效。 */
	D2D1_COLOR_F PartBackColor = D2D1_COLOR_F{ 1, 1, 1, 0.06f };
	/** @brief 固定宽度/自适应分段描边色。 */
	D2D1_COLOR_F PartBorderColor = D2D1_COLOR_F{ 1, 1, 1, 0.10f };
	/** @brief 是否显示顶部细线。 */
	bool ShowTopLine = true;
	/** @brief 是否显示完整外框。 */
	bool ShowBorder = false;
	/** @brief 是否把固定宽度/自适应分段绘制成胶囊。 */
	bool UsePartPills = false;

	/** @brief 创建状态栏。 */
	StatusBar(int x, int y, int width, int height = 26);

	/**
	 * @brief 添加一个分段。
	 * @param text 分段文本。
	 * @param width 分段宽度；0 表示自适应。
	 * @return 新分段索引。
	 */
	int AddPart(const std::wstring& text = L"", int width = 0);
	/** @brief 清空所有分段。 */
	void ClearParts();
	/** @brief 获取分段数量。 */
	int PartCount() const;
	/** @brief 设置指定分段文本。 */
	void SetPartText(int index, const std::wstring& text);
	/** @brief 获取指定分段文本。 */
	std::wstring GetPartText(int index) const;
	/** @brief 获取指定分段宽度。 */
	int GetPartWidth(int index) const;
	/** @brief 设置指定分段宽度。 */
	void SetPartWidth(int index, int width);

	/** @brief 设置左侧默认文本区文本。 */
	void SetLeftText(const std::wstring& text);
	/** @brief 设置右侧默认文本区文本。 */
	void SetRightText(const std::wstring& text);
	/** @brief 获取左侧默认文本区文本。 */
	std::wstring GetLeftText() const;
	/** @brief 获取右侧默认文本区文本。 */
	std::wstring GetRightText() const;

	Label* LeftLabel() const { return _leftLabel; }
	Label* RightLabel() const { return _rightLabel; }

	/** @brief 重新布局分段与分隔线位置。 */
	void LayoutItems();
	void Update() override;
};
