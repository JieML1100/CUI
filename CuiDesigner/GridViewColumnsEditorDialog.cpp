#include "GridViewColumnsEditorDialog.h"
#include <sstream>

namespace
{
	constexpr int COL_NAME = 0;
	constexpr int COL_WIDTH = 1;
	constexpr int COL_TYPE = 2;
	constexpr int COL_CANEDIT = 3;
	constexpr int COL_BUTTONTEXT = 4;
	constexpr int COL_COMBOITEMS = 5;
	constexpr int COL_UP = 6;
	constexpr int COL_DOWN = 7;
	constexpr int COL_DELETE = 8;

	static List<std::wstring> BuildColumnTypeItems()
	{
		List<std::wstring> items;
		items.Add(L"text");
		items.Add(L"image");
		items.Add(L"check");
		items.Add(L"button");
		items.Add(L"combobox");
		return items;
	}

	static int TypeToComboIndex(ColumnType t)
	{
		switch (t)
		{
		case ColumnType::Image: return 1;
		case ColumnType::Check: return 2;
		case ColumnType::Button: return 3;
		case ColumnType::ComboBox: return 4;
		default: return 0;
		}
	}

	static std::vector<std::wstring> SplitByPipe(const std::wstring& s)
	{
		std::vector<std::wstring> parts;
		std::wstring cur;
		for (wchar_t ch : s)
		{
			if (ch == L'|')
			{
				parts.push_back(cur);
				cur.clear();
			}
			else
			{
				cur.push_back(ch);
			}
		}
		parts.push_back(cur);
		return parts;
	}
}

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
	if (t == L"button") { out = ColumnType::Button; return true; }
	if (t == L"combobox" || t == L"combo") { out = ColumnType::ComboBox; return true; }
	return false;
}

void GridViewColumnsEditorDialog::RefreshGridFromTarget()
{
	if (!_grid) return;
	_grid->Rows.Clear();
	if (!_target) return;
	auto typeItems = BuildColumnTypeItems();
	for (int i = 0; i < _target->Columns.Count; i++)
	{
		auto c = _target->Columns[i];
		const int typeIdx = TypeToComboIndex(c.Type);
		GridViewRow r;
		r.Cells.Add(CellValue(c.Name));
		r.Cells.Add(CellValue(std::to_wstring((int)c.Width)));
		CellValue typeCell;
		typeCell.Tag = typeIdx;
		typeCell.Text = (typeIdx >= 0 && typeIdx < typeItems.Count) ? typeItems[typeIdx] : L"text";
		r.Cells.Add(typeCell);
		r.Cells.Add(CellValue(c.CanEdit));
		r.Cells.Add(CellValue(c.ButtonText));
		{
			std::wstring comboItems;
			for (int k = 0; k < c.ComboBoxItems.Count; k++)
			{
				if (k > 0) comboItems += L"|";
				comboItems += c.ComboBoxItems[k];
			}
			r.Cells.Add(CellValue(comboItems));
		}
		r.Cells.Add(CellValue(L""));
		r.Cells.Add(CellValue(L""));
		r.Cells.Add(CellValue(L""));
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
	this->AllowResize = false;

	auto tip = this->AddControl(new Label(L"Type 使用下拉选择；Editable 为选框；右侧按钮列用于上移/下移/删除；ComboItems 用 | 分隔。", 12, 12));
	tip->Size = { 596, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_grid = this->AddControl(new GridView(12, 38, 596, 310));
	_grid->Columns.Clear();
	{
		GridViewColumn c0(L"Name", 200.0f, ColumnType::Text, true);
		GridViewColumn c1(L"Width", 70.0f, ColumnType::Text, true);
		GridViewColumn c2(L"Type", 130.0f, ColumnType::ComboBox, false);
		c2.ComboBoxItems = BuildColumnTypeItems();
		GridViewColumn c3(L"Editable", 70.0f, ColumnType::Check, false);
		GridViewColumn c4(L"ButtonText", 120.0f, ColumnType::Text, true);
		GridViewColumn c5(L"ComboItems", 200.0f, ColumnType::Text, true);
		GridViewColumn c6(L"", 45.0f, ColumnType::Button, false);
		c6.ButtonText = L"上移";
		GridViewColumn c7(L"", 45.0f, ColumnType::Button, false);
		c7.ButtonText = L"下移";
		GridViewColumn c8(L"", 45.0f, ColumnType::Button, false);
		c8.ButtonText = L"删除";

		_grid->Columns.Add(c0);
		_grid->Columns.Add(c1);
		_grid->Columns.Add(c2);
		_grid->Columns.Add(c3);
		_grid->Columns.Add(c4);
		_grid->Columns.Add(c5);
		_grid->Columns.Add(c6);
		_grid->Columns.Add(c7);
		_grid->Columns.Add(c8);
	}
	_grid->AllowUserToAddRows = true;
	RefreshGridFromTarget();

	_grid->OnUserAddedRow += [this](GridView*, int newRowIndex)
		{
			if (!_grid) return;
			if (newRowIndex < 0 || newRowIndex >= _grid->Rows.Count) return;
			auto& row = _grid->Rows[newRowIndex];
			if (row.Cells.Count < 9)
				row.Cells.resize(9);
			row.Cells[COL_NAME].Text = L"Column";
			row.Cells[COL_WIDTH].Text = L"120";
			row.Cells[COL_TYPE].Tag = 0;
			row.Cells[COL_TYPE].Text = L"text";
			row.Cells[COL_CANEDIT].Tag = 1;
			row.Cells[COL_BUTTONTEXT].Text = L"";
			row.Cells[COL_COMBOITEMS].Text = L"";
			_grid->ChangeEditionSelected(COL_NAME, newRowIndex);
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
				_grid->SelectedColumnIndex = COL_NAME;
				_grid->PostRender();
			}
			else if (c == COL_DOWN)
			{
				if (r + 1 >= _grid->Rows.Count) return;
				_grid->Rows.Swap(r, r + 1);
				_grid->SelectedRowIndex = r + 1;
				_grid->SelectedColumnIndex = COL_NAME;
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
					_grid->SelectedColumnIndex = COL_NAME;
				}
				_grid->PostRender();
			}
		};

	_ok = this->AddControl(new Button(L"确定", 12, 398, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 398, 110, 34));

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_grid) { this->Close(); return; }
		_grid->ChangeEditionSelected(-1, -1);

		_target->Columns.Clear();
		auto typeItems = BuildColumnTypeItems();
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
			if (row.Cells.Count > COL_TYPE)
			{
				const __int64 idx = row.Cells[COL_TYPE].Tag;
				if (idx >= 0 && idx < typeItems.Count)
				{
					ColumnType parsed{};
					if (TryParseColumnType(typeItems[(int)idx], parsed))
						type = parsed;
				}
				else
				{
					auto t = Trim(row.Cells[COL_TYPE].Text);
					if (!t.empty())
					{
						ColumnType parsed{};
						if (TryParseColumnType(t, parsed)) type = parsed;
					}
				}
			}

			bool canEdit = true;
			if (row.Cells.Count > COL_CANEDIT)
				canEdit = (row.Cells[COL_CANEDIT].Tag != 0);

			GridViewColumn newCol(name, width, type, canEdit);

			if (type == ColumnType::Button)
			{
				if (row.Cells.Count > COL_BUTTONTEXT)
					newCol.ButtonText = Trim(row.Cells[COL_BUTTONTEXT].Text);
			}
			else if (type == ColumnType::ComboBox)
			{
				if (row.Cells.Count > COL_COMBOITEMS)
				{
					auto raw = Trim(row.Cells[COL_COMBOITEMS].Text);
					auto parts = SplitByPipe(raw);
					for (auto& p : parts)
					{
						auto v = Trim(p);
						if (v.empty()) continue;
						newCol.ComboBoxItems.Add(v);
					}
				}
			}

			_target->Columns.Add(newCol);
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
