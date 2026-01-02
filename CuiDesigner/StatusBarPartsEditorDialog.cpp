#include "StatusBarPartsEditorDialog.h"
#include <algorithm>

namespace
{
	constexpr int COL_TEXT = 0;
	constexpr int COL_WIDTH = 1;
	constexpr int COL_UP = 2;
	constexpr int COL_DOWN = 3;
	constexpr int COL_DELETE = 4;
}

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

void StatusBarPartsEditorDialog::AddRow(const std::wstring& text, const std::wstring& width)
{
	if (!_grid) return;
	GridViewRow r;
	r.Cells.Add(CellValue(text));
	r.Cells.Add(CellValue(width));
	r.Cells.Add(CellValue(L""));
	r.Cells.Add(CellValue(L""));
	r.Cells.Add(CellValue(L""));
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
}

StatusBarPartsEditorDialog::StatusBarPartsEditorDialog(StatusBar* target)
	: Form(L"编辑 StatusBar 分段", POINT{ 260, 260 }, SIZE{ 560, 440 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto tip = this->AddControl(new Label(L"Width: -1=伸缩；0=自动；>0=固定像素。右侧按钮列可上移/下移/删除；点击底部“* 新行”可新增。", 12, 12));
	tip->Size = { 536, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 536, 340));
	_grid->Columns.Clear();
	{
		GridViewColumn c0(L"Text", 250.0f, ColumnType::Text, true);
		GridViewColumn c1(L"Width", 90.0f, ColumnType::Text, true);
		GridViewColumn c2(L"", 60.0f, ColumnType::Button, false);
		c2.ButtonText = L"上移";
		GridViewColumn c3(L"", 60.0f, ColumnType::Button, false);
		c3.ButtonText = L"下移";
		GridViewColumn c4(L"", 60.0f, ColumnType::Button, false);
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
		row.Cells[COL_TEXT].Text = L"";
		row.Cells[COL_WIDTH].Text = L"0";
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
			_grid->PostRender();
		}
	};

	_ok = this->AddControl(new Button(L"确定", 12, 388, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 388, 110, 34));

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_grid) { this->Close(); return; }
		_grid->ChangeEditionSelected(-1, -1);
		_target->ClearParts();
		for (int i = 0; i < _grid->Rows.Count; i++)
		{
			auto& row = _grid->Rows[i];
			std::wstring text = (row.Cells.Count > COL_TEXT) ? Trim(row.Cells[COL_TEXT].Text) : L"";
			int w = (row.Cells.Count > COL_WIDTH) ? ParseInt(row.Cells[COL_WIDTH].Text, 0) : 0;
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

}
