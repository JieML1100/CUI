#pragma once

/**
 * @file TabControlPagesEditorDialog.h
 * @brief TabControlPagesEditorDialog：编辑 TabControl 页面列表的对话框。
 */
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/TabControl.h"
#include "../CUI/GUI/GridView.h"
#include <functional>

class TabControlPagesEditorDialog : public Form
{
public:
	bool Applied = false;
	std::function<void(Control* page)> OnBeforeDeletePage;

	TabControlPagesEditorDialog(TabControl* target);
	~TabControlPagesEditorDialog() = default;

private:
	TabControl* _target = nullptr;
	GridView* _grid = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	void RefreshGridFromTarget();
	void AddRow(const std::wstring& title, TabPage* page);
};
