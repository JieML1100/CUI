#include "TabControlPagesEditorDialog.h"
#include <sstream>
#include <unordered_set>

namespace
{
	constexpr int COL_TITLE = 0;
	constexpr int COL_UP = 1;
	constexpr int COL_DOWN = 2;
	constexpr int COL_DELETE = 3;
}

std::wstring TabControlPagesEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

void TabControlPagesEditorDialog::AddRow(const std::wstring& title, TabPage* page)
{
	if (!_grid) return;
	GridViewRow r;
	CellValue v(title);
	v.Tag = (__int64)page;
	r.Cells.push_back(v);
	r.Cells.push_back(CellValue(L""));
	r.Cells.push_back(CellValue(L""));
	r.Cells.push_back(CellValue(L""));
	_grid->Rows.push_back(r);
}

void TabControlPagesEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.clear();
	if (!_target) return;
	for (int i = 0; i < _target->Count; i++)
	{
		auto* p = (TabPage*)_target->operator[](i);
		AddRow(p ? p->Text : L"", p);
	}
}

TabControlPagesEditorDialog::TabControlPagesEditorDialog(TabControl* target)
	: Form(L"编辑 TabControl 页", POINT{ 240, 240 }, SIZE{ 520, 420 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto tip = this->AddControl(new Label(L"表格内编辑：Title 可直接修改；右侧按钮列可上移/下移/删除；点击底部“* 新行”可新增。", 12, 12));
	tip->Size = { 496, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 496, 318));
	_grid->Columns.clear();
	{
		GridViewColumn c0(L"Title", 290.0f, ColumnType::Text, true);
		GridViewColumn c1(L"", 60.0f, ColumnType::Button, false);
		c1.ButtonText = L"上移";
		GridViewColumn c2(L"", 60.0f, ColumnType::Button, false);
		c2.ButtonText = L"下移";
		GridViewColumn c3(L"", 60.0f, ColumnType::Button, false);
		c3.ButtonText = L"删除";
		_grid->Columns.push_back(c0);
		_grid->Columns.push_back(c1);
		_grid->Columns.push_back(c2);
		_grid->Columns.push_back(c3);
	}
	_grid->AllowUserToAddRows = true;
	RefreshGridFromTarget();

	_grid->OnUserAddedRow += [this](GridView*, int newRowIndex)
	{
		if (!_grid) return;
		if (newRowIndex < 0 || newRowIndex >= _grid->Rows.size()) return;
		auto& row = _grid->Rows[newRowIndex];
		if (row.Cells.size() < 4)
			row.Cells.resize(4);
		row.Cells[COL_TITLE].Text = L"Page";
		row.Cells[COL_TITLE].Tag = 0;
		_grid->ChangeEditionSelected(COL_TITLE, newRowIndex);
	};

	_grid->OnGridViewButtonClick += [this](GridView*, int c, int r)
	{
		if (!_grid) return;
		if (r < 0 || r >= _grid->Rows.size()) return;
		if (c == COL_UP)
		{
			if (r <= 0) return;
			std::swap(_grid->Rows[r], _grid->Rows[r - 1]);
			_grid->SelectedRowIndex = r - 1;
			_grid->SelectedColumnIndex = COL_TITLE;
			_grid->PostRender();
		}
		else if (c == COL_DOWN)
		{
			if (r + 1 >= _grid->Rows.size()) return;
			std::swap(_grid->Rows[r], _grid->Rows[r + 1]);
			_grid->SelectedRowIndex = r + 1;
			_grid->SelectedColumnIndex = COL_TITLE;
			_grid->PostRender();
		}
		else if (c == COL_DELETE)
		{
			_grid->Rows.erase(_grid->Rows.begin() + r);
			if (_grid->Rows.size() <= 0)
			{
				_grid->SelectedRowIndex = -1;
				_grid->SelectedColumnIndex = -1;
			}
			else
			{
				int sel = r;
				if (sel >= _grid->Rows.size()) sel = _grid->Rows.size() - 1;
				_grid->SelectedRowIndex = sel;
				_grid->SelectedColumnIndex = COL_TITLE;
			}
			_grid->PostRender();
		}
	};

	_ok = this->AddControl(new Button(L"确定", 12, 366, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 366, 110, 34));

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_grid) { this->Close(); return; }
		_grid->ChangeEditionSelected(-1, -1);

		std::vector<std::wstring> titles;
		std::vector<TabPage*> desiredPages;
		std::unordered_set<TabPage*> used;

		// 收集目标列表（跳过空标题）
		for (int i = 0; i < _grid->Rows.size(); i++)
		{
			auto& row = _grid->Rows[i];
			if (row.Cells.size() <= COL_TITLE) continue;
			auto title = Trim(row.Cells[COL_TITLE].Text);
			if (title.empty()) continue;

			TabPage* page = (TabPage*)row.Cells[COL_TITLE].Tag;
			if (page && page->Parent != _target) page = nullptr;
			if (!page)
			{
				page = _target->AddPage(title);
			}

			titles.push_back(title);
			desiredPages.push_back(page);
			used.insert(page);
		}
		if (desiredPages.empty())
		{
			auto* page = _target->AddPage(L"Page 1");
			titles.push_back(L"Page 1");
			desiredPages.push_back(page);
			used.insert(page);
		}

		// 删除未被引用的旧页（从后往前）
		for (int i = _target->Count - 1; i >= 0; i--)
		{
			auto* page = (TabPage*)_target->operator[](i);
			if (!page) continue;
			if (used.find(page) != used.end()) continue;
			if (OnBeforeDeletePage) OnBeforeDeletePage(page);
			_target->RemoveControl(page);
			delete page;
		}

		// 重排页顺序：把 desiredPages 交换到目标位置
		for (int i = 0; i < (int)desiredPages.size(); i++)
		{
			TabPage* want = desiredPages[i];
			int cur = -1;
			for (int j = 0; j < _target->Count; j++)
			{
				if (_target->operator[](j) == want) { cur = j; break; }
			}
			if (cur >= 0 && cur != i)
			{
				std::swap(_target->Children[cur], _target->Children[i]);
			}
		}

		// 更新标题
		for (int i = 0; i < (int)titles.size() && i < _target->Count; i++)
		{
			_target->operator[](i)->Text = titles[i];
		}

		if (_target->SelectedIndex < 0) _target->SelectedIndex = 0;
		if (_target->SelectedIndex >= _target->Count) _target->SelectedIndex = _target->Count - 1;

		Applied = true;
		_target->PostRender();
		this->Close();
		};

	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
		};
}
