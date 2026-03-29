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
	if (_grid->Rows.size() <= 0) return;

	int checkedIndex = -1;
	for (int i = 0; i < _grid->Rows.size(); i++)
	{
		auto& row = _grid->Rows[i];
		if (row.Cells.size() <= COL_DEFAULT) continue;
		if (row.Cells[COL_DEFAULT].Tag)
		{
			checkedIndex = i;
			break;
		}
	}

	if (checkedIndex < 0)
	{
		if (_grid->Rows[0].Cells.size() <= COL_DEFAULT)
			_grid->Rows[0].Cells.resize((size_t)COL_DEFAULT + 1);
		_grid->Rows[0].Cells[COL_DEFAULT].Tag = 1;
		checkedIndex = 0;
	}

	for (int i = 0; i < _grid->Rows.size(); i++)
	{
		if (i == checkedIndex) continue;
		auto& row = _grid->Rows[i];
		if (row.Cells.size() <= COL_DEFAULT) continue;
		row.Cells[COL_DEFAULT].Tag = 0;
	}
}

void ComboBoxItemsEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.clear();
	if (!_target) return;
	for (int i = 0; i < _target->Items.size(); i++)
	{
		GridViewRow r;
		r.Cells.push_back(CellValue(false));
		r.Cells.push_back(CellValue(_target->Items[i]));
		r.Cells.push_back(CellValue(L""));
		r.Cells.push_back(CellValue(L""));
		r.Cells.push_back(CellValue(L""));
		r.Cells[COL_DEFAULT].Tag = (i == _target->SelectedIndex) ? 1 : 0;
		_grid->Rows.push_back(r);
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
	_grid->Columns.clear();
	{
		GridViewColumn c0(L"默认", 64.0f, ColumnType::Check, false);
		GridViewColumn c1(L"Item", 270.0f, ColumnType::Text, true);
		GridViewColumn c2(L"↑", 52.0f, ColumnType::Button, false);
		c2.ButtonText = L"上移";
		GridViewColumn c3(L"↓", 52.0f, ColumnType::Button, false);
		c3.ButtonText = L"下移";
		GridViewColumn c4(L"X", 52.0f, ColumnType::Button, false);
		c4.ButtonText = L"删除";
		_grid->Columns.push_back(c0);
		_grid->Columns.push_back(c1);
		_grid->Columns.push_back(c2);
		_grid->Columns.push_back(c3);
		_grid->Columns.push_back(c4);
	}
	_grid->AllowUserToAddRows = true;
	RefreshGridFromTarget();

	_grid->OnUserAddedRow += [this](GridView*, int newRowIndex)
	{
		if (!_grid) return;
		if (newRowIndex < 0 || newRowIndex >= _grid->Rows.size()) return;
		auto& row = _grid->Rows[newRowIndex];
		if (row.Cells.size() < 5)
			row.Cells.resize(5);
		row.Cells[COL_DEFAULT].Tag = 0;
		row.Cells[COL_TEXT].Text = L"";
		if (_grid->Rows.size() == 1)
			row.Cells[COL_DEFAULT].Tag = 1;
		EnsureOneDefaultChecked();
		_grid->ChangeEditionSelected(COL_TEXT, newRowIndex);
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
			_grid->SelectedColumnIndex = COL_TEXT;
			_grid->PostRender();
		}
		else if (c == COL_DOWN)
		{
			if (r + 1 >= _grid->Rows.size()) return;
			std::swap(_grid->Rows[r], _grid->Rows[r + 1]);
			_grid->SelectedRowIndex = r + 1;
			_grid->SelectedColumnIndex = COL_TEXT;
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
		if (r < 0 || r >= _grid->Rows.size()) return;
		(void)v;
		EnsureOneDefaultChecked();
		_grid->PostRender();
	};

	_ok = this->AddControl(new Button(L"确定", 12, 366, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 366, 110, 34));

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_grid) { this->Close(); return; }
		_grid->ChangeEditionSelected(-1, -1);

		_target->Items.clear();
		int selectedIndex = -1;
		for (int i = 0; i < _grid->Rows.size(); i++)
		{
			auto& row = _grid->Rows[i];
			if (row.Cells.size() <= COL_TEXT) continue;
			auto t = Trim(row.Cells[COL_TEXT].Text);
			if (t.empty()) continue;
			const int outIndex = _target->Items.size();
			_target->Items.push_back(t);
			if (row.Cells.size() > COL_DEFAULT && row.Cells[COL_DEFAULT].Tag)
				selectedIndex = outIndex;
		}
		// 防御性修正
		if (selectedIndex < 0) selectedIndex = 0;
		if (selectedIndex >= _target->Items.size()) selectedIndex = std::max(0, (int)_target->Items.size() - 1);
		_target->SelectedIndex = 0;
		if (_target->Items.size() > 0)
		{
			_target->SelectedIndex = selectedIndex;
			_target->Text = _target->Items[_target->SelectedIndex];
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
