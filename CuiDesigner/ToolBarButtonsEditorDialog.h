#pragma once

/**
 * @file ToolBarButtonsEditorDialog.h
 * @brief ToolBarButtonsEditorDialog：编辑 ToolBar 按钮集合的对话框。
 */
#include "../CUI/include/Form.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/GridView.h"
#include <vector>

class ToolBarButtonsEditorDialog : public Form
{
public:
	struct ButtonEdit
	{
		Button* ExistingButton = nullptr;
		std::wstring Text;
		int Width = 90;
	};

	bool Applied = false;
	std::vector<ButtonEdit> Buttons;

	ToolBarButtonsEditorDialog(ToolBar* target);
	~ToolBarButtonsEditorDialog() = default;

private:
	ToolBar* _target = nullptr;
	GridView* _grid = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	void RefreshGridFromTarget();
	void AddRow(
		const std::wstring& text,
		const std::wstring& width,
		Button* button = nullptr);
};
