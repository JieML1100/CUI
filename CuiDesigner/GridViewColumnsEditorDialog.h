#pragma once
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/GridView.h"
#include "../CUI/GUI/TextBox.h"

class GridViewColumnsEditorDialog : public Form
{
public:
	bool Applied = false;

	GridViewColumnsEditorDialog(GridView* target);
	~GridViewColumnsEditorDialog() = default;

private:
	GridView* _target = nullptr;
	GridView* _grid = nullptr;
	Button* _add = nullptr;
	Button* _remove = nullptr;
	Button* _up = nullptr;
	Button* _down = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	static bool TryParseColumnType(const std::wstring& s, ColumnType& out);
	void RefreshGridFromTarget();
	void SyncButtons();
	void AddColumnRow(const std::wstring& name, const std::wstring& width, const std::wstring& type);
	void RemoveSelected();
	void MoveSelected(int delta);
};
