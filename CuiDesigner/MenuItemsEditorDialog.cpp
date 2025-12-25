#include "MenuItemsEditorDialog.h"
#include <algorithm>

std::wstring MenuItemsEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

bool MenuItemsEditorDialog::ParseBool(const std::wstring& s, bool def)
{
	auto t = Trim(s);
	if (t.empty()) return def;
	std::wstring low = t;
	std::transform(low.begin(), low.end(), low.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
	if (low == L"1" || low == L"true" || low == L"yes" || low == L"y") return true;
	if (low == L"0" || low == L"false" || low == L"no" || low == L"n") return false;
	return def;
}

int MenuItemsEditorDialog::ParseInt(const std::wstring& s, int def)
{
	auto t = Trim(s);
	if (t.empty()) return def;
	try { return std::stoi(t); }
	catch (...) { return def; }
}

void MenuItemsEditorDialog::LoadModelFromTarget()
{
	_tops.clear();
	_currentTopIndex = -1;
	if (!_target) return;

	for (int i = 0; i < _target->Count; i++)
	{
		auto* top = dynamic_cast<MenuItem*>(_target->operator[](i));
		if (!top) continue;
		ItemModel m;
		m.Text = top->Text;
		m.Id = top->Id;
		m.Shortcut = top->Shortcut;
		m.Separator = top->Separator;
		m.Enable = top->Enable;
		m.SubItems.reserve(top->SubItems.size());
		for (auto* si : top->SubItems)
		{
			if (!si) continue;
			ItemModel sm;
			sm.Text = si->Text;
			sm.Id = si->Id;
			sm.Shortcut = si->Shortcut;
			sm.Separator = si->Separator;
			sm.Enable = si->Enable;
			m.SubItems.push_back(std::move(sm));
		}
		_tops.push_back(std::move(m));
	}
}

void MenuItemsEditorDialog::RefreshTopGrid()
{
	if (!_topGrid) return;
	_topGrid->Rows.Clear();
	for (auto& t : _tops)
	{
		GridViewRow r;
		r.Cells.Add(CellValue(t.Text));
		_topGrid->Rows.Add(r);
	}
	if (_tops.empty())
	{
		_topGrid->SelectedRowIndex = -1;
		_topGrid->SelectedColumnIndex = -1;
		_currentTopIndex = -1;
		if (_subGrid) _subGrid->Rows.Clear();
	}
	else
	{
		int idx = _currentTopIndex;
		if (idx < 0) idx = 0;
		if (idx >= (int)_tops.size()) idx = (int)_tops.size() - 1;
		_topGrid->SelectedRowIndex = idx;
		_topGrid->SelectedColumnIndex = 0;
		_currentTopIndex = idx;
		LoadSubGridFromModel(idx);
	}
	SyncButtons();
}

void MenuItemsEditorDialog::LoadSubGridFromModel(int topIndex)
{
	if (!_subGrid) return;
	_subGrid->Rows.Clear();
	if (topIndex < 0 || topIndex >= (int)_tops.size()) return;
	auto& subs = _tops[topIndex].SubItems;
	for (auto& s : subs)
	{
		GridViewRow r;
		r.Cells.Add(CellValue(s.Text));
		r.Cells.Add(CellValue(std::to_wstring(s.Id)));
		r.Cells.Add(CellValue(s.Shortcut));
		r.Cells.Add(CellValue(s.Separator ? L"true" : L"false"));
		r.Cells.Add(CellValue(s.Enable ? L"true" : L"false"));
		_subGrid->Rows.Add(r);
	}
	_subGrid->SelectedRowIndex = (_subGrid->Rows.Count > 0) ? 0 : -1;
	_subGrid->SelectedColumnIndex = (_subGrid->Rows.Count > 0) ? 0 : -1;
	SyncButtons();
}

void MenuItemsEditorDialog::SaveSubGridToModel(int topIndex)
{
	if (!_subGrid) return;
	if (topIndex < 0 || topIndex >= (int)_tops.size()) return;

	auto& subs = _tops[topIndex].SubItems;
	subs.clear();
	subs.reserve(_subGrid->Rows.Count);
	for (int i = 0; i < _subGrid->Rows.Count; i++)
	{
		auto& row = _subGrid->Rows[i];
		ItemModel m;
		m.Text = (row.Cells.Count > 0) ? Trim(row.Cells[0].Text) : L"";
		m.Id = (row.Cells.Count > 1) ? ParseInt(row.Cells[1].Text, 0) : 0;
		m.Shortcut = (row.Cells.Count > 2) ? Trim(row.Cells[2].Text) : L"";
		m.Separator = (row.Cells.Count > 3) ? ParseBool(row.Cells[3].Text, false) : false;
		m.Enable = (row.Cells.Count > 4) ? ParseBool(row.Cells[4].Text, true) : true;
		// Separator 行允许 Text 为空；非 Separator 行要求 Text 非空
		if (!m.Separator && m.Text.empty())
			continue;
		subs.push_back(std::move(m));
	}
}

void MenuItemsEditorDialog::SyncButtons()
{
	if (_topGrid && _topRemove && _topUp && _topDown)
	{
		int idx = _topGrid->SelectedRowIndex;
		bool hasSel = (idx >= 0 && idx < _topGrid->Rows.Count);
		_topRemove->Enable = hasSel;
		_topUp->Enable = hasSel && idx > 0;
		_topDown->Enable = hasSel && idx + 1 < _topGrid->Rows.Count;
	}
	if (_subGrid && _subRemove && _subUp && _subDown)
	{
		int idx = _subGrid->SelectedRowIndex;
		bool hasSel = (idx >= 0 && idx < _subGrid->Rows.Count);
		_subRemove->Enable = hasSel;
		_subUp->Enable = hasSel && idx > 0;
		_subDown->Enable = hasSel && idx + 1 < _subGrid->Rows.Count;
	}
}

void MenuItemsEditorDialog::SetCurrentTopIndex(int idx)
{
	if (idx == _currentTopIndex) return;
	// 切换前保存当前子项编辑
	SaveSubGridToModel(_currentTopIndex);
	_currentTopIndex = idx;
	LoadSubGridFromModel(_currentTopIndex);
	SyncButtons();
}

void MenuItemsEditorDialog::AddTop()
{
	ItemModel m;
	m.Text = L"Menu";
	m.Enable = true;
	_tops.push_back(std::move(m));
	_currentTopIndex = (int)_tops.size() - 1;
	RefreshTopGrid();
	if (_topGrid)
	{
		_topGrid->SelectedRowIndex = _currentTopIndex;
		_topGrid->SelectedColumnIndex = 0;
		_topGrid->ChangeEditionSelected(0, _currentTopIndex);
	}
}

void MenuItemsEditorDialog::RemoveTop()
{
	if (!_topGrid) return;
	int idx = _topGrid->SelectedRowIndex;
	if (idx < 0 || idx >= (int)_tops.size()) return;
	_tops.erase(_tops.begin() + idx);
	if (_tops.empty()) _currentTopIndex = -1;
	else
	{
		if (idx >= (int)_tops.size()) idx = (int)_tops.size() - 1;
		_currentTopIndex = idx;
	}
	RefreshTopGrid();
}

void MenuItemsEditorDialog::MoveTop(int delta)
{
	if (!_topGrid) return;
	int idx = _topGrid->SelectedRowIndex;
	int to = idx + delta;
	if (idx < 0 || idx >= (int)_tops.size()) return;
	if (to < 0 || to >= (int)_tops.size()) return;
	std::swap(_tops[idx], _tops[to]);
	_currentTopIndex = to;
	RefreshTopGrid();
}

void MenuItemsEditorDialog::AddSubRow(bool separator)
{
	if (!_subGrid) return;
	GridViewRow r;
	r.Cells.Add(CellValue(separator ? L"" : L"Item"));
	r.Cells.Add(CellValue(L"0"));
	r.Cells.Add(CellValue(L""));
	r.Cells.Add(CellValue(separator ? L"true" : L"false"));
	r.Cells.Add(CellValue(L"true"));
	_subGrid->Rows.Add(r);
	_subGrid->SelectedRowIndex = _subGrid->Rows.Count - 1;
	_subGrid->SelectedColumnIndex = 0;
	_subGrid->ChangeEditionSelected(0, _subGrid->SelectedRowIndex);
	SyncButtons();
}

void MenuItemsEditorDialog::RemoveSub()
{
	if (!_subGrid) return;
	int idx = _subGrid->SelectedRowIndex;
	if (idx < 0 || idx >= _subGrid->Rows.Count) return;
	_subGrid->Rows.RemoveAt(idx);
	if (_subGrid->Rows.Count <= 0)
	{
		_subGrid->SelectedRowIndex = -1;
		_subGrid->SelectedColumnIndex = -1;
	}
	else
	{
		if (idx >= _subGrid->Rows.Count) idx = _subGrid->Rows.Count - 1;
		_subGrid->SelectedRowIndex = idx;
		_subGrid->SelectedColumnIndex = 0;
	}
	SyncButtons();
}

void MenuItemsEditorDialog::MoveSub(int delta)
{
	if (!_subGrid) return;
	int idx = _subGrid->SelectedRowIndex;
	int to = idx + delta;
	if (idx < 0 || idx >= _subGrid->Rows.Count) return;
	if (to < 0 || to >= _subGrid->Rows.Count) return;
	_subGrid->Rows.Swap(idx, to);
	_subGrid->SelectedRowIndex = to;
	_subGrid->SelectedColumnIndex = 0;
	SyncButtons();
}

void MenuItemsEditorDialog::ApplyToTarget()
{
	if (!_target) return;

	// 先将 UI 中的编辑同步到模型
	SaveSubGridToModel(_currentTopIndex);

	// 清空现有顶层 MenuItem（会递归释放 SubItems）
	while (_target->Count > 0)
	{
		auto* c = _target->operator[](_target->Count - 1);
		_target->RemoveControl(c);
		delete c;
	}

	for (auto& t : _tops)
	{
		auto text = Trim(t.Text);
		if (text.empty()) continue;
		auto* top = _target->AddItem(text);
		if (!top) continue;
		top->Id = t.Id;
		top->Shortcut = t.Shortcut;
		top->Enable = t.Enable;

		for (auto& s : t.SubItems)
		{
			if (s.Separator)
			{
				top->AddSeparator();
				continue;
			}
			auto st = Trim(s.Text);
			if (st.empty()) continue;
			auto* si = top->AddSubItem(st, s.Id);
			if (!si) continue;
			si->Shortcut = s.Shortcut;
			si->Enable = s.Enable;
		}
	}

	_target->PostRender();
}

MenuItemsEditorDialog::MenuItemsEditorDialog(Menu* target)
	: Form(L"编辑 Menu 菜单项", POINT{ 260, 260 }, SIZE{ 760, 520 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;

	auto tip = this->AddControl(new Label(L"左侧编辑菜单栏项；右侧编辑该项的下拉项。Separator 填 true 可插入分隔线。", 12, 12));
	tip->Size = { 736, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_topGrid = this->AddControl(new GridView(12, 38, 260, 360));
	_topGrid->Columns.Clear();
	_topGrid->Columns.Add(GridViewColumn(L"Top Items", 220.0f, ColumnType::Text, true));

	_subGrid = this->AddControl(new GridView(284, 38, 464, 360));
	_subGrid->Columns.Clear();
	_subGrid->Columns.Add(GridViewColumn(L"Text", 170.0f, ColumnType::Text, true));
	_subGrid->Columns.Add(GridViewColumn(L"Id", 60.0f, ColumnType::Text, true));
	_subGrid->Columns.Add(GridViewColumn(L"Shortcut", 110.0f, ColumnType::Text, true));
	_subGrid->Columns.Add(GridViewColumn(L"Separator", 70.0f, ColumnType::Text, true));
	_subGrid->Columns.Add(GridViewColumn(L"Enable", 60.0f, ColumnType::Text, true));

	_topAdd = this->AddControl(new Button(L"新增", 12, 406, 62, 30));
	_topRemove = this->AddControl(new Button(L"删除", 78, 406, 62, 30));
	_topUp = this->AddControl(new Button(L"上移", 144, 406, 62, 30));
	_topDown = this->AddControl(new Button(L"下移", 210, 406, 62, 30));

	_subAdd = this->AddControl(new Button(L"新增项", 284, 406, 90, 30));
	auto* subSep = this->AddControl(new Button(L"新增分隔", 380, 406, 110, 30));
	_subRemove = this->AddControl(new Button(L"删除", 496, 406, 70, 30));
	_subUp = this->AddControl(new Button(L"上移", 572, 406, 70, 30));
	_subDown = this->AddControl(new Button(L"下移", 648, 406, 70, 30));

	_ok = this->AddControl(new Button(L"确定", 12, 448, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 448, 110, 34));

	LoadModelFromTarget();
	RefreshTopGrid();

	_topGrid->SelectionChanged += [this](Control*) {
		if (!_topGrid) return;
		int idx = _topGrid->SelectedRowIndex;
		SetCurrentTopIndex(idx);
	};
	_subGrid->SelectionChanged += [this](Control*) { SyncButtons(); };

	_topAdd->OnMouseClick += [this](Control*, MouseEventArgs) { AddTop(); };
	_topRemove->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveTop(); };
	_topUp->OnMouseClick += [this](Control*, MouseEventArgs) { MoveTop(-1); };
	_topDown->OnMouseClick += [this](Control*, MouseEventArgs) { MoveTop(+1); };

	_subAdd->OnMouseClick += [this](Control*, MouseEventArgs) { AddSubRow(false); };
	subSep->OnMouseClick += [this](Control*, MouseEventArgs) { AddSubRow(true); };
	_subRemove->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveSub(); };
	_subUp->OnMouseClick += [this](Control*, MouseEventArgs) { MoveSub(-1); };
	_subDown->OnMouseClick += [this](Control*, MouseEventArgs) { MoveSub(+1); };

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		ApplyToTarget();
		Applied = true;
		this->Close();
	};
	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
	};

	SyncButtons();
}
