#pragma once

/**
 * @file ToolBox.h
 * @brief ToolBox：设计器工具箱（控件类型选择）。
 */
#include "../CUI/GUI/Panel.h"
#include "../CUI/GUI/Button.h"
#include "DesignerTypes.h"
#include <functional>

class ToolBoxItem : public Button
{
public:
	UIClass ControlType;
	const char* SvgData = nullptr;
	int BaseY = 0;

	ToolBoxItem(std::wstring text, UIClass type, const char* svg, int x, int y, int width = 120, int height = 30)
		: Button(text, x, y, width, height), ControlType(type), SvgData(svg), BaseY(y)
	{
		this->Round = 0.15f;
	}

	~ToolBoxItem() override;
	void Update() override;

	ID2D1Bitmap* GetIcon() const { return _iconBitmap; }
	void EnsureIcon();

private:
	ID2D1Bitmap* _iconBitmap = nullptr;
	
};

class ToolBox : public Panel
{
private:
	std::vector<ToolBoxItem*> _items;
	class Label* _titleLabel = nullptr;
	Panel* _itemsHost = nullptr;
	int _contentTop = 45;
	int _contentBottomPadding = 10;
	int _scrollOffsetY = 0;
	int _contentHeight = 0;
	bool _draggingScrollThumb = false;
	int _dragStartMouseY = 0;
	int _dragStartScrollY = 0;

	void UpdateScrollLayout();
	void ClampScroll();
	bool TryGetScrollBarLocalRect(D2D1_RECT_F& outTrack, D2D1_RECT_F& outThumb);
	
public:
	ToolBox(int x, int y, int width, int height);
	virtual ~ToolBox();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
	
	Event<void(UIClass)> OnControlSelected;
};
