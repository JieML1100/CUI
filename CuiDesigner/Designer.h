#pragma once

/**
 * @file Designer.h
 * @brief Designer：CUI 可视化设计器主窗口。
 */
#include "../CUI/GUI/Form.h"
#include "DesignerCanvas.h"
#include "ToolBox.h"
#include "PropertyGrid.h"
#include "CodeGenerator.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/Label.h"

class Designer : public Form
{
private:
	ToolBox* _toolBox;
	DesignerCanvas* _canvas;
	PropertyGrid* _propertyGrid;
	
	// 顶部工具栏
	Button* _btnNew;
	Button* _btnOpen;
	Button* _btnSave;
	Button* _btnExport;
	Button* _btnDelete;
	Label* _lblInfo;
	
	void OnToolBoxControlSelected(UIClass type);
	void OnCanvasControlSelected(std::shared_ptr<DesignerControl> control);
	void OnNewClick();
	void OnOpenClick();
	void OnSaveClick();
	void OnExportClick();
	void OnDeleteClick();
	
	std::wstring _currentFileName;
	
public:
	Designer();
	virtual ~Designer();
	
	void InitializeComponents();
	void InitAndShow(); // 初始化并显示窗口
};
