#include "ToolBarButtonsEditorDialog.h"
#include <sstream>

std::wstring ToolBarButtonsEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

void ToolBarButtonsEditorDialog::SyncButtons()
{
	if (!_grid || !_remove || !_up || !_down) return;
	int idx = _grid->SelectedRowIndex;
	bool hasSel = (idx >= 0 && idx < _grid->Rows.Count);
	_remove->Enable = hasSel;
	_up->Enable = hasSel && idx > 0;
	_down->Enable = hasSel && idx >= 0 && idx + 1 < _grid->Rows.Count;
}

void ToolBarButtonsEditorDialog::AddRow(const std::wstring& text, const std::wstring& width)
{
	if (!_grid) return;
	GridViewRow r;
	r.Cells.Add(CellValue(text));
	r.Cells.Add(CellValue(width));
	_grid->Rows.Add(r);
}

void ToolBarButtonsEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.Clear();
	if (!_target) return;
	for (int i = 0; i < _target->Count; i++)
	{
		auto c = _target->operator[](i);
		if (!c) continue;
		AddRow(c->Text, std::to_wstring((int)c->Width));
	}
}

void ToolBarButtonsEditorDialog::RemoveSelected()
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

void ToolBarButtonsEditorDialog::MoveSelected(int delta)
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

ToolBarButtonsEditorDialog::ToolBarButtonsEditorDialog(ToolBar* target)
	: Form(L"编辑 ToolBar 按钮", POINT{ 260, 260 }, SIZE{ 560, 440 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;

	auto tip = this->AddControl(new Label(L"双击单元格可编辑；可新增/删除/重排。", 12, 12));
	tip->Size = { 536, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 536, 300));
	_grid->Columns.Clear();
	_grid->Columns.Add(GridViewColumn(L"Text", 300.0f, ColumnType::Text, true));
	_grid->Columns.Add(GridViewColumn(L"Width", 90.0f, ColumnType::Text, true));
	RefreshGridFromTarget();
	_grid->SelectionChanged += [this](Control*) { SyncButtons(); };

	_add = this->AddControl(new Button(L"新增", 12, 346, 90, 30));
	_remove = this->AddControl(new Button(L"删除", 108, 346, 90, 30));
	_up = this->AddControl(new Button(L"上移", 204, 346, 90, 30));
	_down = this->AddControl(new Button(L"下移", 300, 346, 90, 30));

	_ok = this->AddControl(new Button(L"确定", 12, 388, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 388, 110, 34));

	SyncButtons();
	_add->OnMouseClick += [this](Control*, MouseEventArgs) {
		AddRow(L"Button", L"90");
		if (_grid)
		{
			_grid->SelectedRowIndex = _grid->Rows.Count - 1;
			_grid->SelectedColumnIndex = 0;
			_grid->ChangeEditionSelected(0, _grid->SelectedRowIndex);
		}
		SyncButtons();
	};
	_remove->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveSelected(); };
	_up->OnMouseClick += [this](Control*, MouseEventArgs) { MoveSelected(-1); };
	_down->OnMouseClick += [this](Control*, MouseEventArgs) { MoveSelected(+1); };

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_grid) { this->Close(); return; }

		// 清空现有按钮（递归释放子控件）
		while (_target->Count > 0)
		{
			auto c = _target->operator[](_target->Count - 1);
			if (OnBeforeDeleteButton) OnBeforeDeleteButton(c);
			_target->RemoveControl(c);
			delete c;
		}

		for (int i = 0; i < _grid->Rows.Count; i++)
		{
			auto& row = _grid->Rows[i];
			std::wstring text = (row.Cells.Count > 0) ? Trim(row.Cells[0].Text) : L"";
			if (text.empty()) continue;
			int width = 90;
			if (row.Cells.Count > 1)
			{
				auto w = Trim(row.Cells[1].Text);
				if (!w.empty())
				{
					try { width = std::stoi(w); }
					catch (...) { width = 90; }
					if (width < 24) width = 24;
				}
			}
			_target->AddToolButton(text, width);
		}

		_target->LayoutItems();
		Applied = true;
		_target->PostRender();
		this->Close();
	};

	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
	};
}
