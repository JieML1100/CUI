#include "StatusBarPartsEditorDialog.h"
#include <algorithm>

std::wstring StatusBarPartsEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

int StatusBarPartsEditorDialog::ParseInt(const std::wstring& s, int def)
{
	auto t = Trim(s);
	if (t.empty()) return def;
	try { return std::stoi(t); }
	catch (...) { return def; }
}

void StatusBarPartsEditorDialog::SyncButtons()
{
	if (!_grid || !_remove || !_up || !_down) return;
	int idx = _grid->SelectedRowIndex;
	bool hasSel = (idx >= 0 && idx < _grid->Rows.Count);
	_remove->Enable = hasSel;
	_up->Enable = hasSel && idx > 0;
	_down->Enable = hasSel && idx + 1 < _grid->Rows.Count;
}

void StatusBarPartsEditorDialog::AddRow(const std::wstring& text, const std::wstring& width)
{
	if (!_grid) return;
	GridViewRow r;
	r.Cells.Add(CellValue(text));
	r.Cells.Add(CellValue(width));
	_grid->Rows.Add(r);
}

void StatusBarPartsEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.Clear();
	if (!_target) return;
	if (_target->PartCount() <= 0)
	{
		// 确保至少两段（兼容 Left/Right）
		_target->SetLeftText(L"");
		_target->SetRightText(L"");
	}
	for (int i = 0; i < _target->PartCount(); i++)
	{
		std::wstring text = _target->GetPartText(i);
		int w = _target->GetPartWidth(i);
		AddRow(text, std::to_wstring(w));
	}
	_grid->SelectedRowIndex = (_grid->Rows.Count > 0) ? 0 : -1;
	_grid->SelectedColumnIndex = (_grid->Rows.Count > 0) ? 0 : -1;
	SyncButtons();
}

void StatusBarPartsEditorDialog::RemoveSelected()
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

void StatusBarPartsEditorDialog::MoveSelected(int delta)
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

StatusBarPartsEditorDialog::StatusBarPartsEditorDialog(StatusBar* target)
	: Form(L"编辑 StatusBar 分段", POINT{ 260, 260 }, SIZE{ 560, 440 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto tip = this->AddControl(new Label(L"Width: -1=伸缩；0=自动；>0=固定像素。", 12, 12));
	tip->Size = { 536, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 536, 300));
	_grid->Columns.Clear();
	_grid->Columns.Add(GridViewColumn(L"Text", 340.0f, ColumnType::Text, true));
	_grid->Columns.Add(GridViewColumn(L"Width", 120.0f, ColumnType::Text, true));

	RefreshGridFromTarget();
	_grid->SelectionChanged += [this](Control*) { SyncButtons(); };

	_add = this->AddControl(new Button(L"新增", 12, 346, 90, 30));
	_remove = this->AddControl(new Button(L"删除", 108, 346, 90, 30));
	_up = this->AddControl(new Button(L"上移", 204, 346, 90, 30));
	_down = this->AddControl(new Button(L"下移", 300, 346, 90, 30));

	_ok = this->AddControl(new Button(L"确定", 12, 388, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 388, 110, 34));

	_add->OnMouseClick += [this](Control*, MouseEventArgs) {
		AddRow(L"", L"0");
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
		_target->ClearParts();
		for (int i = 0; i < _grid->Rows.Count; i++)
		{
			auto& row = _grid->Rows[i];
			std::wstring text = (row.Cells.Count > 0) ? Trim(row.Cells[0].Text) : L"";
			int w = (row.Cells.Count > 1) ? ParseInt(row.Cells[1].Text, 0) : 0;
			_target->AddPart(text, w);
		}
		_target->LayoutItems();
		_target->PostRender();
		Applied = true;
		this->Close();
	};
	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
	};

	SyncButtons();
}
