#pragma once
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/Menu.h"
#include "../CUI/GUI/GridView.h"

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
	GridView* _topGrid = nullptr;
	GridView* _subGrid = nullptr;
	Button* _topAdd = nullptr;
	Button* _topRemove = nullptr;
	Button* _topUp = nullptr;
	Button* _topDown = nullptr;
	Button* _subAdd = nullptr;
	Button* _subRemove = nullptr;
	Button* _subUp = nullptr;
	Button* _subDown = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	std::vector<ItemModel> _tops;
	int _currentTopIndex = -1;

	static std::wstring Trim(const std::wstring& s);
	static bool ParseBool(const std::wstring& s, bool def);
	static int ParseInt(const std::wstring& s, int def);

	void LoadModelFromTarget();
	void RefreshTopGrid();
	void LoadSubGridFromModel(int topIndex);
	void SaveSubGridToModel(int topIndex);
	void SyncButtons();
	void SetCurrentTopIndex(int idx);
	void ApplyToTarget();

	void AddTop();
	void RemoveTop();
	void MoveTop(int delta);

	void AddSubRow(bool separator);
	void RemoveSub();
	void MoveSub(int delta);
};
