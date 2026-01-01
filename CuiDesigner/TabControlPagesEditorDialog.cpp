#include "TabControlPagesEditorDialog.h"
#include <sstream>
#include <unordered_set>

std::wstring TabControlPagesEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

void TabControlPagesEditorDialog::SyncButtons()
{
	if (!_grid || !_remove || !_up || !_down) return;
	int idx = _grid->SelectedRowIndex;
	bool hasSel = (idx >= 0 && idx < _grid->Rows.Count);
	_remove->Enable = hasSel;
	_up->Enable = hasSel && idx > 0;
	_down->Enable = hasSel && idx >= 0 && idx + 1 < _grid->Rows.Count;
}

void TabControlPagesEditorDialog::AddRow(const std::wstring& title, TabPage* page)
{
	if (!_grid) return;
	GridViewRow r;
	CellValue v(title);
	v.Tag = (__int64)page;
	r.Cells.Add(v);
	_grid->Rows.Add(r);
}

void TabControlPagesEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.Clear();
	if (!_target) return;
	for (int i = 0; i < _target->Count; i++)
	{
		auto* p = (TabPage*)_target->operator[](i);
		AddRow(p ? p->Text : L"", p);
	}
}

void TabControlPagesEditorDialog::RemoveSelected()
{
	if (!_grid) return;
	int idx = _grid->SelectedRowIndex;
	if (idx < 0 || idx >= _grid->Rows.Count) return;
	_grid->Rows.RemoveAt(idx);
	if (_grid->Rows.Count <= 0)
	{
		_grid->SelectedRowIndex = -1;
		_grid->SelectedColumnIndex = -1;
	}
	else
	{
		if (idx >= _grid->Rows.Count) idx = _grid->Rows.Count - 1;
		_grid->SelectedRowIndex = idx;
		_grid->SelectedColumnIndex = 0;
	}
	SyncButtons();
}

void TabControlPagesEditorDialog::MoveSelected(int delta)
{
	if (!_grid) return;
	int idx = _grid->SelectedRowIndex;
	int to = idx + delta;
	if (idx < 0 || idx >= _grid->Rows.Count) return;
	if (to < 0 || to >= _grid->Rows.Count) return;
	_grid->Rows.Swap(idx, to);
	_grid->SelectedRowIndex = to;
	_grid->SelectedColumnIndex = 0;
	SyncButtons();
}

TabControlPagesEditorDialog::TabControlPagesEditorDialog(TabControl* target)
	: Form(L"编辑 TabControl 页", POINT{ 240, 240 }, SIZE{ 520, 420 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto tip = this->AddControl(new Label(L"双击单元格可编辑；删除/上移/下移可调整页。", 12, 12));
	tip->Size = { 496, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 496, 282));
	_grid->Columns.Clear();
	_grid->Columns.Add(GridViewColumn(L"Title", 460.0f, ColumnType::Text, true));
	RefreshGridFromTarget();
	_grid->SelectionChanged += [this](Control*) { SyncButtons(); };

	_add = this->AddControl(new Button(L"新增", 12, 328, 90, 30));
	_remove = this->AddControl(new Button(L"删除", 108, 328, 90, 30));
	_up = this->AddControl(new Button(L"上移", 204, 328, 90, 30));
	_down = this->AddControl(new Button(L"下移", 300, 328, 90, 30));

	_ok = this->AddControl(new Button(L"确定", 12, 366, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 366, 110, 34));

	SyncButtons();

	_add->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_grid) return;
		GridViewRow r;
		CellValue v(L"Page");
		v.Tag = 0;
		r.Cells.Add(v);
		_grid->Rows.Add(r);
		_grid->SelectedRowIndex = _grid->Rows.Count - 1;
		_grid->SelectedColumnIndex = 0;
		_grid->ChangeEditionSelected(0, _grid->SelectedRowIndex);
		SyncButtons();
		};
	_remove->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveSelected(); };
	_up->OnMouseClick += [this](Control*, MouseEventArgs) { MoveSelected(-1); };
	_down->OnMouseClick += [this](Control*, MouseEventArgs) { MoveSelected(+1); };

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_grid) { this->Close(); return; }

		std::vector<std::wstring> titles;
		std::vector<TabPage*> desiredPages;
		std::unordered_set<TabPage*> used;

		// 收集目标列表（跳过空标题）
		for (int i = 0; i < _grid->Rows.Count; i++)
		{
			auto& row = _grid->Rows[i];
			if (row.Cells.Count <= 0) continue;
			auto title = Trim(row.Cells[0].Text);
			if (title.empty()) continue;

			TabPage* page = (TabPage*)row.Cells[0].Tag;
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
				_target->Children.Swap(cur, i);
			}
		}

		// 更新标题
		for (int i = 0; i < (int)titles.size() && i < _target->Count; i++)
		{
			_target->operator[](i)->Text = titles[i];
		}

		if (_target->SelectIndex < 0) _target->SelectIndex = 0;
		if (_target->SelectIndex >= _target->Count) _target->SelectIndex = _target->Count - 1;

		Applied = true;
		_target->PostRender();
		this->Close();
		};

	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
		};
}
