#pragma once

/**
 * @file StatusBarPartsEditorDialog.h
 * @brief StatusBarPartsEditorDialog：编辑 StatusBar 分段的对话框。
 */
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/StatusBar.h"
#include "../CUI/GUI/GridView.h"

class StatusBarPartsEditorDialog : public Form
{
public:
	bool Applied = false;

	StatusBarPartsEditorDialog(StatusBar* target);
	~StatusBarPartsEditorDialog() = default;

private:
	StatusBar* _target = nullptr;
	GridView* _grid = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	static int ParseInt(const std::wstring& s, int def);
	void RefreshGridFromTarget();
	void AddRow(const std::wstring& text, const std::wstring& width);
};
