#include "GridPanelDefinitionsEditorDialog.h"
#include <sstream>

std::wstring GridPanelDefinitionsEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

std::vector<std::wstring> GridPanelDefinitionsEditorDialog::SplitLines(const std::wstring& text)
{
	std::vector<std::wstring> lines;
	std::wstring current;
	for (size_t i = 0; i < text.size(); i++)
	{
		wchar_t c = text[i];
		if (c == L'\r')
		{
			if (i + 1 < text.size() && text[i + 1] == L'\n') i++;
			lines.push_back(current);
			current.clear();
			continue;
		}
		if (c == L'\n')
		{
			lines.push_back(current);
			current.clear();
			continue;
		}
		current.push_back(c);
	}
	lines.push_back(current);
	return lines;
}

bool GridPanelDefinitionsEditorDialog::TryParseGridLength(const std::wstring& token, GridLength& out)
{
	auto t = Trim(token);
	if (t.empty()) return false;
	std::wstring lower = t;
	for (auto& ch : lower) ch = (wchar_t)towlower(ch);
	if (lower == L"auto")
	{
		out = GridLength::Auto();
		return true;
	}
	// star: "*" or "2*"
	if (lower.back() == L'*')
	{
		std::wstring num = lower.substr(0, lower.size() - 1);
		float factor = 1.0f;
		if (!num.empty())
		{
			try { factor = (float)std::stof(num); }
			catch (...) { factor = 1.0f; }
			if (factor <= 0.0f) factor = 1.0f;
		}
		out = GridLength::Star(factor);
		return true;
	}
	// pixel
	try
	{
		float px = (float)std::stof(lower);
		if (px < 0.0f) px = 0.0f;
		out = GridLength::Pixels(px);
		return true;
	}
	catch (...) {}
	return false;
}

std::wstring GridPanelDefinitionsEditorDialog::GridLengthToString(const GridLength& gl)
{
	std::wstringstream ss;
	switch (gl.Unit)
	{
	case SizeUnit::Auto:
		return L"auto";
	case SizeUnit::Star:
		if (gl.Value == 1.0f) return L"*";
		ss << gl.Value << L"*";
		return ss.str();
	case SizeUnit::Pixel:
	default:
		ss << gl.Value;
		return ss.str();
	}
}

std::wstring GridPanelDefinitionsEditorDialog::JoinRows(GridPanel* gp)
{
	if (!gp) return L"";
	std::wstringstream ss;
	auto& rows = gp->GetRows();
	for (size_t i = 0; i < rows.size(); i++)
	{
		ss << GridLengthToString(rows[i].Height);
		if (i + 1 < rows.size()) ss << L"\r\n";
	}
	return ss.str();
}

std::wstring GridPanelDefinitionsEditorDialog::JoinCols(GridPanel* gp)
{
	if (!gp) return L"";
	std::wstringstream ss;
	auto& cols = gp->GetColumns();
	for (size_t i = 0; i < cols.size(); i++)
	{
		ss << GridLengthToString(cols[i].Width);
		if (i + 1 < cols.size()) ss << L"\r\n";
	}
	return ss.str();
}

GridPanelDefinitionsEditorDialog::GridPanelDefinitionsEditorDialog(GridPanel* target)
	: Form(L"编辑 GridPanel 行/列", POINT{ 300, 300 }, SIZE{ 720, 460 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto tip = this->AddControl(new Label(L"每行一个定义：auto / 数字(像素) / *(星号) / 2*", 12, 12));
	tip->Size = { 690, 20 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	auto rowLabel = this->AddControl(new Label(L"Rows", 12, 42));
	rowLabel->Size = { 330, 18 };
	auto colLabel = this->AddControl(new Label(L"Columns", 366, 42));
	colLabel->Size = { 330, 18 };

	_rows = this->AddControl(new RichTextBox(L"", 12, 64, 336, 320));
	_rows->AllowMultiLine = true;
	_rows->BackColor = Colors::White;
	_rows->FocusedColor = D2D1_COLOR_F{ 1,1,1,1 };
	_rows->Text = JoinRows(_target);

	_cols = this->AddControl(new RichTextBox(L"", 366, 64, 336, 320));
	_cols->AllowMultiLine = true;
	_cols->BackColor = Colors::White;
	_cols->FocusedColor = D2D1_COLOR_F{ 1,1,1,1 };
	_cols->Text = JoinCols(_target);

	_ok = this->AddControl(new Button(L"确定", 12, 396, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 396, 110, 34));

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_rows || !_cols) { this->Close(); return; }

		auto rowLines = SplitLines(_rows->Text);
		auto colLines = SplitLines(_cols->Text);

		_target->ClearRows();
		_target->ClearColumns();

		int addedRows = 0;
		for (auto& raw : rowLines)
		{
			auto t = Trim(raw);
			if (t.empty()) continue;
			GridLength gl;
			if (TryParseGridLength(t, gl))
			{
				_target->AddRow(gl);
				addedRows++;
			}
		}
		int addedCols = 0;
		for (auto& raw : colLines)
		{
			auto t = Trim(raw);
			if (t.empty()) continue;
			GridLength gl;
			if (TryParseGridLength(t, gl))
			{
				_target->AddColumn(gl);
				addedCols++;
			}
		}

		// 防御：至少 1x1
		if (addedRows <= 0) _target->AddRow(GridLength::Star(1.0f));
		if (addedCols <= 0) _target->AddColumn(GridLength::Star(1.0f));

		Applied = true;
		_target->PostRender();
		this->Close();
	};

	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
	};
}
