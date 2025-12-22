#pragma once
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/ComboBox.h"
#include "../CUI/GUI/GridView.h"
#include "../CUI/GUI/TextBox.h"

class ComboBoxItemsEditorDialog : public Form
{
public:
	bool Applied = false;

	ComboBoxItemsEditorDialog(ComboBox* target);
	~ComboBoxItemsEditorDialog() = default;

private:
	ComboBox* _target = nullptr;
	GridView* _grid = nullptr;
	TextBox* _newItem = nullptr;
	Button* _add = nullptr;
	Button* _remove = nullptr;
	Button* _up = nullptr;
	Button* _down = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	void RefreshGridFromTarget();
	void SyncButtons();
	void AddItem(const std::wstring& text);
	void RemoveSelected();
	void MoveSelected(int delta);
	static std::wstring Trim(std::wstring s);
};
