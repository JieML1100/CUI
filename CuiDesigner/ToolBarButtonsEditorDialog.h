#pragma once

/**
 * @file ToolBarButtonsEditorDialog.h
 * @brief ToolBarButtonsEditorDialog：编辑 ToolBar 按钮集合的对话框。
 */
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/ToolBar.h"
#include "../CUI/GUI/GridView.h"
#include <functional>

class ToolBarButtonsEditorDialog : public Form
{
public:
	bool Applied = false;
	std::function<void(Control* button)> OnBeforeDeleteButton;

	ToolBarButtonsEditorDialog(ToolBar* target);
	~ToolBarButtonsEditorDialog() = default;

private:
	ToolBar* _target = nullptr;
	GridView* _grid = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	void RefreshGridFromTarget();
	void AddRow(const std::wstring& text, const std::wstring& width);
};
