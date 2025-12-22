#include "GridViewColumnsEditorDialog.h"
#include <sstream>

std::wstring GridViewColumnsEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

bool GridViewColumnsEditorDialog::TryParseColumnType(const std::wstring& s, ColumnType& out)
{
	auto t = Trim(s);
	for (auto& ch : t) ch = (wchar_t)towlower(ch);
	if (t == L"text") { out = ColumnType::Text; return true; }
	if (t == L"image") { out = ColumnType::Image; return true; }
	if (t == L"check") { out = ColumnType::Check; return true; }
	return false;
}

void GridViewColumnsEditorDialog::SyncButtons()
{
	if (!_grid || !_remove || !_up || !_down) return;
	int idx = _grid->SelectedRowIndex;
	bool hasSel = (idx >= 0 && idx < _grid->Rows.Count);
	_remove->Enable = hasSel;
	_up->Enable = hasSel && idx > 0;
	_down->Enable = hasSel && idx >= 0 && idx + 1 < _grid->Rows.Count;
}

void GridViewColumnsEditorDialog::AddColumnRow(const std::wstring& name, const std::wstring& width, const std::wstring& type)
{
	if (!_grid) return;
	GridViewRow r;
	r.Cells.Add(CellValue(name));
	r.Cells.Add(CellValue(width));
	r.Cells.Add(CellValue(type));
	_grid->Rows.Add(r);
	_grid->SelectedRowIndex = _grid->Rows.Count - 1;
	_grid->SelectedColumnIndex = 0;
	_grid->ChangeEditionSelected(0, _grid->SelectedRowIndex);
	SyncButtons();
}

void GridViewColumnsEditorDialog::RemoveSelected()
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

void GridViewColumnsEditorDialog::MoveSelected(int delta)
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

void GridViewColumnsEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.Clear();
	if (!_target) return;
	for (int i = 0; i < _target->Columns.Count; i++)
	{
		auto c = _target->Columns[i];
		std::wstring type = L"text";
		switch (c.Type)
		{
		case ColumnType::Image: type = L"image"; break;
		case ColumnType::Check: type = L"check"; break;
		default: break;
		}
		GridViewRow r;
		r.Cells.Add(CellValue(c.Name));
		r.Cells.Add(CellValue(std::to_wstring((int)c.Width)));
		r.Cells.Add(CellValue(type));
		_grid->Rows.Add(r);
	}
}

GridViewColumnsEditorDialog::GridViewColumnsEditorDialog(GridView* target)
	: Form(L"编辑 GridView 列", POINT{ 220, 220 }, SIZE{ 620, 460 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;

	auto tip = this->AddControl(new Label(L"双击单元格可编辑；type 支持 text / image / check", 12, 12));
	tip->Size = { 596, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 596, 310));
	_grid->Columns.Clear();
	_grid->Columns.Add(GridViewColumn(L"Name", 260.0f, ColumnType::Text, true));
	_grid->Columns.Add(GridViewColumn(L"Width", 90.0f, ColumnType::Text, true));
	_grid->Columns.Add(GridViewColumn(L"Type", 140.0f, ColumnType::Text, true));
	RefreshGridFromTarget();
	_grid->SelectionChanged += [this](Control*) {
		SyncButtons();
	};

	_add = this->AddControl(new Button(L"新增", 12, 356, 90, 30));
	_remove = this->AddControl(new Button(L"删除", 108, 356, 90, 30));
	_up = this->AddControl(new Button(L"上移", 204, 356, 90, 30));
	_down = this->AddControl(new Button(L"下移", 300, 356, 90, 30));

	_ok = this->AddControl(new Button(L"确定", 12, 398, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 398, 110, 34));

	SyncButtons();

	_add->OnMouseClick += [this](Control*, MouseEventArgs) {
		AddColumnRow(L"Column", L"120", L"text");
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

		_target->Columns.Clear();
		for (int i = 0; i < _grid->Rows.Count; i++)
		{
			auto& row = _grid->Rows[i];
			std::wstring name = (row.Cells.Count > 0) ? Trim(row.Cells[0].Text) : L"";
			if (name.empty()) continue;

			float width = 120.0f;
			if (row.Cells.Count > 1)
			{
				auto w = Trim(row.Cells[1].Text);
				if (!w.empty())
				{
					try { width = (float)std::stoi(w); }
					catch (...) { width = 120.0f; }
					if (width < 24.0f) width = 24.0f;
				}
			}

			ColumnType type = ColumnType::Text;
			if (row.Cells.Count > 2)
			{
				auto t = Trim(row.Cells[2].Text);
				if (!t.empty())
				{
					ColumnType parsed{};
					if (TryParseColumnType(t, parsed)) type = parsed;
				}
			}

			_target->Columns.Add(GridViewColumn(name, width, type, true));
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
