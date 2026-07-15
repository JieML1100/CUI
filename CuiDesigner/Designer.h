#pragma once

/**
 * @file Designer.h
 * @brief Designer：CUI 可视化设计器主窗口。
 */
#include "../CUI/include/Form.h"
#include "DesignerCanvas.h"
#include "ToolBox.h"
#include "PropertyGrid.h"
#include "CodeGenerator.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/Label.h"
class Designer : public Form
{
private:
	ToolBox* _toolBox = nullptr;
	DesignerCanvas* _canvas = nullptr;
	PropertyGrid* _propertyGrid = nullptr;
	
	// 顶部工具栏
	Button* _btnNew = nullptr;
	Button* _btnOpen = nullptr;
	Button* _btnSave = nullptr;
	Button* _btnExport = nullptr;
	Button* _btnDelete = nullptr;
	Label* _lblInfo = nullptr;
	std::shared_ptr<IBindingSource> _designDataContext;
	
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
	void SetDesignDataContext(std::shared_ptr<IBindingSource> source);
};
