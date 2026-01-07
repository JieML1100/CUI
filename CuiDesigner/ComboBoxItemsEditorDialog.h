#pragma once

/**
 * @file ComboBoxItemsEditorDialog.h
 * @brief ComboBoxItemsEditorDialog：编辑 ComboBox 下拉项的对话框。
 */
#include "../CUI_Legacy/GUI/Form.h"
#include "../CUI_Legacy/GUI/Label.h"
#include "../CUI_Legacy/GUI/Button.h"
#include "../CUI_Legacy/GUI/ComboBox.h"
#include "../CUI_Legacy/GUI/GridView.h"

class ComboBoxItemsEditorDialog : public Form
{
public:
	bool Applied = false;

	ComboBoxItemsEditorDialog(ComboBox* target);
	~ComboBoxItemsEditorDialog() = default;

private:
	ComboBox* _target = nullptr;
	GridView* _grid = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	void RefreshGridFromTarget();
	void EnsureOneDefaultChecked();
	static std::wstring Trim(std::wstring s);
};
