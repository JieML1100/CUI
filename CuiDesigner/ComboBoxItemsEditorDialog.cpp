#include "ComboBoxItemsEditorDialog.h"
#include <sstream>

std::wstring ComboBoxItemsEditorDialog::Trim(std::wstring s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

void ComboBoxItemsEditorDialog::SyncButtons()
{
	if (!_grid || !_remove || !_up || !_down) return;
	int idx = _grid->SelectedRowIndex;
	bool hasSel = (idx >= 0 && idx < _grid->Rows.Count);
	_remove->Enable = hasSel;
	_up->Enable = hasSel && idx > 0;
	_down->Enable = hasSel && idx >= 0 && idx + 1 < _grid->Rows.Count;
}

void ComboBoxItemsEditorDialog::AddItem(const std::wstring& text)
{
	if (!_grid) return;
	GridViewRow r;
	r.Cells.Add(CellValue(text));
	_grid->Rows.Add(r);
	_grid->SelectedRowIndex = _grid->Rows.Count - 1;
	_grid->SelectedColumnIndex = 0;
	_grid->ChangeEditionSelected(0, _grid->SelectedRowIndex);
	SyncButtons();
}

void ComboBoxItemsEditorDialog::RemoveSelected()
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

void ComboBoxItemsEditorDialog::MoveSelected(int delta)
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

void ComboBoxItemsEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.Clear();
	if (!_target) return;
	for (int i = 0; i < _target->values.Count; i++)
	{
		GridViewRow r;
		r.Cells.Add(CellValue(_target->values[i]));
		_grid->Rows.Add(r);
	}
}

ComboBoxItemsEditorDialog::ComboBoxItemsEditorDialog(ComboBox* target)
	: Form(L"编辑 ComboBox 下拉项", POINT{ 200, 200 }, SIZE{ 520, 420 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto title = this->AddControl(new Label(L"双击单元格可编辑；也可在下方输入后新增。", 12, 12));
	title->Size = { 496, 20 };
	title->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 496, 272));
	_grid->Columns.Clear();
	_grid->Columns.Add(GridViewColumn(L"Item", 460.0f, ColumnType::Text, true));
	RefreshGridFromTarget();
	_grid->SelectionChanged += [this](Control*) {
		SyncButtons();
	};

	_newItem = this->AddControl(new TextBox(L"", 12, 318, 320, 28));
	_add = this->AddControl(new Button(L"新增", 340, 318, 80, 28));
	_remove = this->AddControl(new Button(L"删除", 428, 318, 80, 28));

	_up = this->AddControl(new Button(L"上移", 340, 350, 80, 28));
	_down = this->AddControl(new Button(L"下移", 428, 350, 80, 28));

	_ok = this->AddControl(new Button(L"确定", 12, 366, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 366, 110, 34));

	SyncButtons();

	_add->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_grid) return;
		std::wstring t = _newItem ? Trim(_newItem->Text) : L"";
		AddItem(t);
		if (_newItem) _newItem->Text = L"";
	};
	_remove->OnMouseClick += [this](Control*, MouseEventArgs) {
		RemoveSelected();
	};
	_up->OnMouseClick += [this](Control*, MouseEventArgs) {
		MoveSelected(-1);
	};
	_down->OnMouseClick += [this](Control*, MouseEventArgs) {
		MoveSelected(+1);
	};

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_grid) { this->Close(); return; }

		_target->values.Clear();
		for (int i = 0; i < _grid->Rows.Count; i++)
		{
			if (_grid->Rows[i].Cells.Count <= 0) continue;
			auto t = Trim(_grid->Rows[i].Cells[0].Text);
			if (t.empty()) continue;
			_target->values.Add(t);
		}
		// 防御性修正
		if (_target->SelectedIndex < 0) _target->SelectedIndex = 0;
		if (_target->SelectedIndex >= _target->values.Count) _target->SelectedIndex = std::max(0, _target->values.Count - 1);
		if (_target->values.Count > 0)
		{
			_target->Text = _target->values[_target->SelectedIndex];
		}

		Applied = true;
		_target->PostRender();
		this->Close();
	};

	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
	};
}
