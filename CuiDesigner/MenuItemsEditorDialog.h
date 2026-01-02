#pragma once

/**
 * @file MenuItemsEditorDialog.h
 * @brief MenuItemsEditorDialog：编辑菜单项结构的对话框。
 */
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/RichTextBox.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/Menu.h"

class MenuItemsEditorDialog : public Form
{
public:
	bool Applied = false;

	MenuItemsEditorDialog(Menu* target);
	~MenuItemsEditorDialog() = default;

private:
	struct ItemModel
	{
		std::wstring Text;
		int Id = 0;
		std::wstring Shortcut;
		bool Separator = false;
		bool Enable = true;
		std::vector<ItemModel> SubItems;
	};

	Menu* _target = nullptr;
	RichTextBox* _editor = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	std::vector<ItemModel> _tops;

	static std::wstring Trim(const std::wstring& s);
	static std::vector<std::wstring> SplitLines(const std::wstring& text);
	static int CountIndentTabs(const std::wstring& s);
	static std::wstring StripIndentTabs(const std::wstring& s);
	static bool ParseBool(const std::wstring& s, bool def);
	static int ParseInt(const std::wstring& s, int def);
	static std::vector<std::wstring> SplitByPipe(const std::wstring& s);

	void LoadModelFromTarget();
	static void SerializeItems(std::wstringstream& ss, const std::vector<ItemModel>& items, int depth);
	static std::wstring ModelToText(const std::vector<ItemModel>& tops);
	static std::vector<ItemModel> TextToModel(const std::wstring& text);
	static ItemModel FromMenuItem(MenuItem* it);
	static void ApplySubItems(MenuItem* parent, const std::vector<ItemModel>& subs);
	void ApplyToTarget();
};
