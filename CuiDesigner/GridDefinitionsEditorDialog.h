#pragma once

/**
 * @file GridDefinitionsEditorDialog.h
 * @brief GridDefinitionsEditorDialog：编辑 Grid 行/列定义的对话框。
 */
#include "../CUI/include/Window.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/Layout/Grid.h"

class GridDefinitionsEditorDialog : public Window
{
public:
	bool Applied = false;

	GridDefinitionsEditorDialog(Grid* target);
	~GridDefinitionsEditorDialog() = default;

private:
	Grid* _target = nullptr;
	RichTextBox* _rows = nullptr;
	RichTextBox* _cols = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	static std::vector<std::wstring> SplitLines(const std::wstring& text);
	static bool TryParseGridLength(const std::wstring& token, GridLength& out);
	static std::wstring GridLengthToString(const GridLength& gl);
	static std::wstring JoinRows(Grid* grid);
	static std::wstring JoinCols(Grid* grid);
};
