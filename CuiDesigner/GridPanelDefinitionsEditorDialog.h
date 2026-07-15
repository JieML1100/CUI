#pragma once

/**
 * @file GridPanelDefinitionsEditorDialog.h
 * @brief GridPanelDefinitionsEditorDialog：编辑 GridPanel 行/列定义的对话框。
 */
#include "../CUI/include/Form.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/Layout/GridPanel.h"

class GridPanelDefinitionsEditorDialog : public Form
{
public:
	bool Applied = false;

	GridPanelDefinitionsEditorDialog(GridPanel* target);
	~GridPanelDefinitionsEditorDialog() = default;

private:
	GridPanel* _target = nullptr;
	RichTextBox* _rows = nullptr;
	RichTextBox* _cols = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	static std::vector<std::wstring> SplitLines(const std::wstring& text);
	static bool TryParseGridLength(const std::wstring& token, GridLength& out);
	static std::wstring GridLengthToString(const GridLength& gl);
	static std::wstring JoinRows(GridPanel* gridPanel);
	static std::wstring JoinCols(GridPanel* gridPanel);
};
