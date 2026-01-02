#include "ComboBoxItemsEditorDialog.h"
#include <sstream>

namespace
{
	constexpr int COL_DEFAULT = 0;
	constexpr int COL_TEXT = 1;
	constexpr int COL_UP = 2;
	constexpr int COL_DOWN = 3;
	constexpr int COL_DELETE = 4;
}

std::wstring ComboBoxItemsEditorDialog::Trim(std::wstring s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

void ComboBoxItemsEditorDialog::EnsureOneDefaultChecked()
{
	if (!_grid) return;
	if (_grid->Rows.Count <= 0) return;

	int checkedIndex = -1;
	for (int i = 0; i < _grid->Rows.Count; i++)
	{
		auto& row = _grid->Rows[i];
		if (row.Cells.Count <= COL_DEFAULT) continue;
		if (row.Cells[COL_DEFAULT].Tag)
		{
			checkedIndex = i;
			break;
		}
	}

	if (checkedIndex < 0)
	{
		if (_grid->Rows[0].Cells.Count <= COL_DEFAULT)
			_grid->Rows[0].Cells.resize((size_t)COL_DEFAULT + 1);
		_grid->Rows[0].Cells[COL_DEFAULT].Tag = 1;
		checkedIndex = 0;
	}

	for (int i = 0; i < _grid->Rows.Count; i++)
	{
		if (i == checkedIndex) continue;
		auto& row = _grid->Rows[i];
		if (row.Cells.Count <= COL_DEFAULT) continue;
		row.Cells[COL_DEFAULT].Tag = 0;
	}
}

void ComboBoxItemsEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.Clear();
	if (!_target) return;
	for (int i = 0; i < _target->values.Count; i++)
	{
		GridViewRow r;
		r.Cells.Add(CellValue(false));
		r.Cells.Add(CellValue(_target->values[i]));
		r.Cells.Add(CellValue(L""));
		r.Cells.Add(CellValue(L""));
		r.Cells.Add(CellValue(L""));
		r.Cells[COL_DEFAULT].Tag = (i == _target->SelectedIndex) ? 1 : 0;
		_grid->Rows.Add(r);
	}
	EnsureOneDefaultChecked();
}

ComboBoxItemsEditorDialog::ComboBoxItemsEditorDialog(ComboBox* target)
	: Form(L"编辑 ComboBox 下拉项", POINT{ 200, 200 }, SIZE{ 520, 420 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto title = this->AddControl(new Label(L"在表格内直接编辑：勾选默认项；按钮列可上移/下移/删除；点击底部“* 新行”可新增。", 12, 12));
	title->Size = { 496, 20 };
	title->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 496, 318));
	_grid->Columns.Clear();
	{
		GridViewColumn c0(L"默认", 64.0f, ColumnType::Check, false);
		GridViewColumn c1(L"Item", 270.0f, ColumnType::Text, true);
		GridViewColumn c2(L"↑", 52.0f, ColumnType::Button, false);
		c2.ButtonText = L"上移";
		GridViewColumn c3(L"↓", 52.0f, ColumnType::Button, false);
		c3.ButtonText = L"下移";
		GridViewColumn c4(L"X", 52.0f, ColumnType::Button, false);
		c4.ButtonText = L"删除";
		_grid->Columns.Add(c0);
		_grid->Columns.Add(c1);
		_grid->Columns.Add(c2);
		_grid->Columns.Add(c3);
		_grid->Columns.Add(c4);
	}
	_grid->AllowUserToAddRows = true;
	RefreshGridFromTarget();

	_grid->OnUserAddedRow += [this](GridView*, int newRowIndex)
	{
		if (!_grid) return;
		if (newRowIndex < 0 || newRowIndex >= _grid->Rows.Count) return;
		auto& row = _grid->Rows[newRowIndex];
		if (row.Cells.Count < 5)
			row.Cells.resize(5);
		row.Cells[COL_DEFAULT].Tag = 0;
		row.Cells[COL_TEXT].Text = L"";
		if (_grid->Rows.Count == 1)
			row.Cells[COL_DEFAULT].Tag = 1;
		EnsureOneDefaultChecked();
		_grid->ChangeEditionSelected(COL_TEXT, newRowIndex);
	};

	_grid->OnGridViewButtonClick += [this](GridView*, int c, int r)
	{
		if (!_grid) return;
		if (r < 0 || r >= _grid->Rows.Count) return;
		if (c == COL_UP)
		{
			if (r <= 0) return;
			_grid->Rows.Swap(r, r - 1);
			_grid->SelectedRowIndex = r - 1;
			_grid->SelectedColumnIndex = COL_TEXT;
			_grid->PostRender();
		}
		else if (c == COL_DOWN)
		{
			if (r + 1 >= _grid->Rows.Count) return;
			_grid->Rows.Swap(r, r + 1);
			_grid->SelectedRowIndex = r + 1;
			_grid->SelectedColumnIndex = COL_TEXT;
			_grid->PostRender();
		}
		else if (c == COL_DELETE)
		{
			_grid->Rows.RemoveAt(r);
			if (_grid->Rows.Count <= 0)
			{
				_grid->SelectedRowIndex = -1;
				_grid->SelectedColumnIndex = -1;
			}
			else
			{
				int sel = r;
				if (sel >= _grid->Rows.Count) sel = _grid->Rows.Count - 1;
				_grid->SelectedRowIndex = sel;
				_grid->SelectedColumnIndex = COL_TEXT;
			}
			EnsureOneDefaultChecked();
			_grid->PostRender();
		}
	};

	_grid->OnGridViewCheckStateChanged += [this](GridView*, int c, int r, bool v)
	{
		if (!_grid) return;
		if (c != COL_DEFAULT) return;
		if (r < 0 || r >= _grid->Rows.Count) return;
		(void)v;
		EnsureOneDefaultChecked();
		_grid->PostRender();
	};

	_ok = this->AddControl(new Button(L"确定", 12, 366, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 366, 110, 34));

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_grid) { this->Close(); return; }
		_grid->ChangeEditionSelected(-1, -1);

		_target->values.Clear();
		int selectedIndex = -1;
		for (int i = 0; i < _grid->Rows.Count; i++)
		{
			auto& row = _grid->Rows[i];
			if (row.Cells.Count <= COL_TEXT) continue;
			auto t = Trim(row.Cells[COL_TEXT].Text);
			if (t.empty()) continue;
			const int outIndex = _target->values.Count;
			_target->values.Add(t);
			if (row.Cells.Count > COL_DEFAULT && row.Cells[COL_DEFAULT].Tag)
				selectedIndex = outIndex;
		}
		// 防御性修正
		if (selectedIndex < 0) selectedIndex = 0;
		if (selectedIndex >= _target->values.Count) selectedIndex = std::max(0, _target->values.Count - 1);
		_target->SelectedIndex = 0;
		if (_target->values.Count > 0)
		{
			_target->SelectedIndex = selectedIndex;
			_target->Text = _target->values[_target->SelectedIndex];
		}
		else
		{
			_target->Text = L"";
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
