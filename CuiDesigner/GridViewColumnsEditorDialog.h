#pragma once

/**
 * @file GridViewColumnsEditorDialog.h
 * @brief GridViewColumnsEditorDialog：编辑 GridView 列配置的对话框。
 */
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/GridView.h"

class GridViewColumnsEditorDialog : public Form
{
public:
	bool Applied = false;

	GridViewColumnsEditorDialog(GridView* target);
	~GridViewColumnsEditorDialog() = default;

private:
	GridView* _target = nullptr;
	GridView* _grid = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	static bool TryParseColumnType(const std::wstring& s, ColumnType& out);
	void RefreshGridFromTarget();
};
