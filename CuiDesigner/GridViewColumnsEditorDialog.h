#pragma once

/**
 * @file GridViewColumnsEditorDialog.h
 * @brief GridViewColumnsEditorDialog：编辑 GridView 列配置的对话框。
 */
#include "../CUI/include/Form.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/GridView.h"

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
