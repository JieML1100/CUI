#pragma once

/**
 * @file PropertyGrid.h
 * @brief PropertyGrid：设计器属性面板（显示/编辑控件属性）。
 */
#include "../CUI/GUI/Panel.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/CheckBox.h"
#include "../CUI/GUI/TextBox.h"
#include "../CUI/GUI/Button.h"
#include "DesignerTypes.h"
#include <memory>

class DesignerCanvas;

class PropertyItem
{
public:
	std::wstring PropertyName;
	Label* NameLabel;
	Control* ValueControl;
	TextBox* ValueTextBox;
	CheckBox* ValueCheckBox;
	
	PropertyItem(std::wstring name, Label* label, TextBox* textBox)
		: PropertyName(name), NameLabel(label), ValueControl(textBox), ValueTextBox(textBox), ValueCheckBox(nullptr)
	{
	}

	PropertyItem(std::wstring name, Label* label, CheckBox* checkBox)
		: PropertyName(name), NameLabel(label), ValueControl(checkBox), ValueTextBox(nullptr), ValueCheckBox(checkBox)
	{
	}

	PropertyItem(std::wstring name, Label* label, Control* valueControl)
		: PropertyName(name), NameLabel(label), ValueControl(valueControl), ValueTextBox(nullptr), ValueCheckBox(nullptr)
	{
	}
};

class PropertyGrid : public Panel
{
private:
	std::vector<PropertyItem*> _items;
	std::vector<Control*> _extraControls;
	Panel* _contentHost = nullptr;
	struct ScrollEntry
	{
		Control* ControlPtr = nullptr;
		int BaseY = 0;
	};
	std::vector<ScrollEntry> _scrollEntries;
	int _contentTop = 45;
	int _contentBottomPadding = 10;
	int _scrollOffsetY = 0;
	int _contentHeight = 0;
	bool _draggingScrollThumb = false;
	int _dragStartMouseY = 0;
	int _dragStartScrollY = 0;

	std::shared_ptr<DesignerControl> _currentControl;
	DesignerCanvas* _canvas = nullptr;
	Label* _titleLabel;

	Panel* GetContentContainer();
	int GetContentTopLocal();
	int GetContentWidthLocal();
	int GetViewportHeightLocal();
	void UpdateContentHostLayout();

	void RegisterScrollable(Control* c);
	void UpdateScrollLayout();
	void ClampScroll();
	bool TryGetScrollBarLocalRect(D2D1_RECT_F& outTrack, D2D1_RECT_F& outThumb);
	
	void CreatePropertyItem(std::wstring propertyName, std::wstring value, int& yOffset);
	void CreateColorPropertyItem(std::wstring propertyName, const D2D1_COLOR_F& value, int& yOffset);
	void CreateThicknessPropertyItem(std::wstring propertyName, const Thickness& value, int& yOffset);
	void CreateBoolPropertyItem(std::wstring propertyName, bool value, int& yOffset);
	void CreateEventBoolPropertyItem(std::wstring eventName, bool enabled, int& yOffset);
	void CreateAnchorPropertyItem(std::wstring propertyName, uint8_t anchorStyles, int& yOffset);
	void CreateEnumPropertyItem(std::wstring propertyName, const std::wstring& value,
		const std::vector<std::wstring>& options, int& yOffset);
	void CreateFloatSliderPropertyItem(std::wstring propertyName, float value,
		float minValue, float maxValue, float step, int& yOffset);
	void UpdatePropertyFromTextBox(std::wstring propertyName, std::wstring value);
	void UpdatePropertyFromBool(std::wstring propertyName, bool value);
	void UpdatePropertyFromFloat(std::wstring propertyName, float value);
	void UpdateAnchorFromChecks(bool left, bool top, bool right, bool bottom);
	
public:
	PropertyGrid(int x, int y, int width, int height);
	virtual ~PropertyGrid();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
	
	void SetDesignerCanvas(DesignerCanvas* canvas) { _canvas = canvas; }
	void LoadControl(std::shared_ptr<DesignerControl> control);
	void Clear();
};
