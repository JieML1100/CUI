#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file PasswordBox.h
 * @brief PasswordBox：密码输入框。
 *
 * 行为概览：
 * - 负责处理输入、选择、光标与水平滚动
 * - 显示层面通常会对文本进行掩码渲染（实现见 cpp）
 */
class PasswordBox : public Control
{
public:
	virtual UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override { (void)xof; (void)yof; return this->Enable ? CursorKind::IBeam : CursorKind::Arrow; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	/** @brief 鼠标悬停时背景色（实现可能会用到）。 */
	D2D1_COLOR_F UnderMouseColor = Colors::White;
	/** @brief 选区背景色。 */
	D2D1_COLOR_F SelectedBackColor = { 0.f , 0.f , 1.f , 0.5f };
	/** @brief 选区前景色。 */
	D2D1_COLOR_F SelectedForeColor = Colors::White;
	/** @brief 获得焦点时高亮色。 */
	D2D1_COLOR_F FocusedColor = Colors::White;
	/** @brief 滚动条背景色（如实现启用）。 */
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	/** @brief 滚动条前景色（如实现启用）。 */
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	/** @brief 当前文本测量尺寸缓存。 */
	D2D1_SIZE_F textSize = { 0,0 };
	/** @brief 选择起始索引（基于字符）。 */
	int SelectionStart = 0;
	/** @brief 选择结束索引（基于字符）。 */
	int SelectionEnd = 0;
	/** @brief 边框宽度（像素）。 */
	float Boder = 1.5f;
	/** @brief 水平滚动偏移（像素）。 */
	float OffsetX = 0.0f;
	/** @brief 文本内边距（像素）。 */
	float TextMargin = 5.0f;
protected:
		D2D1_RECT_F _caretRectCache = { 0,0,0,0 };
	bool _caretRectCacheValid = false;
public:
	/** @brief 创建密码输入框。 */
	PasswordBox(std::wstring text, int x, int y, int width = 120, int height = 24);
private:
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void UpdateScroll(bool arrival = false);
public:
	/** @brief 获取当前选择文本。 */
	std::wstring GetSelectedString();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};