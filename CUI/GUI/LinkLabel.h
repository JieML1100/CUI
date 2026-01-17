#pragma once
#pragma once
#include "Label.h"

/**
 * @file LinkLabel.h
 * @brief LinkLabel：可点击的链接文本控件。
 */
class LinkLabel : public Label
{
public:
	virtual UIClass Type();
	/** @brief 鼠标悬停时文本颜色。 */
	D2D1_COLOR_F HoverColor = Colors::DeepSkyBlue;
	/** @brief 访问/点击后的文本颜色。 */
	D2D1_COLOR_F VisitedColor = Colors::SlateBlue;
	/** @brief 下划线颜色。 */
	D2D1_COLOR_F UnderlineColor = Colors::DeepSkyBlue;
	/** @brief 访问状态。 */
	bool Visited = false;
	/** @brief 创建 LinkLabel。 */
	LinkLabel(std::wstring text, int x, int y);
	void Update() override;
	CursorKind QueryCursor(int xof, int yof) override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
