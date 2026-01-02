#pragma once

/**
 * @file ComboBoxItemsEditorDialog.h
 * @brief ComboBoxItemsEditorDialog：编辑 ComboBox 下拉项的对话框。
 */
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/ComboBox.h"
#include "../CUI/GUI/GridView.h"

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
