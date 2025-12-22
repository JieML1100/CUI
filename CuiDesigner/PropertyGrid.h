#pragma once
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
	TextBox* ValueTextBox;
	CheckBox* ValueCheckBox;
	
	PropertyItem(std::wstring name, Label* label, TextBox* textBox)
		: PropertyName(name), NameLabel(label), ValueTextBox(textBox), ValueCheckBox(nullptr)
	{
	}

	PropertyItem(std::wstring name, Label* label, CheckBox* checkBox)
		: PropertyName(name), NameLabel(label), ValueTextBox(nullptr), ValueCheckBox(checkBox)
	{
	}
};

class PropertyGrid : public Panel
{
private:
	std::vector<PropertyItem*> _items;
	std::vector<Control*> _extraControls;
	std::shared_ptr<DesignerControl> _currentControl;
	DesignerCanvas* _canvas = nullptr;
	Label* _titleLabel;
	
	void CreatePropertyItem(std::wstring propertyName, std::wstring value, int& yOffset);
	void CreateBoolPropertyItem(std::wstring propertyName, bool value, int& yOffset);
	void UpdatePropertyFromTextBox(std::wstring propertyName, std::wstring value);
	void UpdatePropertyFromBool(std::wstring propertyName, bool value);
	
public:
	PropertyGrid(int x, int y, int width, int height);
	virtual ~PropertyGrid();
	
	void SetDesignerCanvas(DesignerCanvas* canvas) { _canvas = canvas; }
	void LoadControl(std::shared_ptr<DesignerControl> control);
	void Clear();
};
