#include "GridDefinitionsEditorDialog.h"
#include "ProgrammaticControlFactory.h"
#include <cmath>
#include <sstream>

std::wstring GridDefinitionsEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

std::vector<std::wstring> GridDefinitionsEditorDialog::SplitLines(const std::wstring& text)
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

bool GridDefinitionsEditorDialog::TryParseGridLength(const std::wstring& token, GridLength& out)
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
		size_t parsed = 0;
		float px = (float)std::stof(lower, &parsed);
		if (parsed != lower.size() || !std::isfinite(px)) return false;
		if (px < 0.0f) px = 0.0f;
		out = GridLength::Pixels(px);
		return true;
	}
	catch (...) {}
	return false;
}

std::wstring GridDefinitionsEditorDialog::GridLengthToString(const GridLength& gl)
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

std::wstring GridDefinitionsEditorDialog::JoinRows(Grid* grid)
{
	if (!grid) return L"";
	std::wstringstream ss;
	auto& rows = grid->GetRows();
	for (size_t i = 0; i < rows.size(); i++)
	{
		ss << GridLengthToString(rows[i].Height);
		if (i + 1 < rows.size()) ss << L"\r\n";
	}
	return ss.str();
}

std::wstring GridDefinitionsEditorDialog::JoinCols(Grid* grid)
{
	if (!grid) return L"";
	std::wstringstream ss;
	auto& columns = grid->GetColumns();
	for (size_t i = 0; i < columns.size(); i++)
	{
		ss << GridLengthToString(columns[i].Width);
		if (i + 1 < columns.size()) ss << L"\r\n";
	}
	return ss.str();
}

GridDefinitionsEditorDialog::GridDefinitionsEditorDialog(Grid* target)
	: Window(), _target(target)
{
	this->Title = L"编辑 Grid 行/列";
	this->Left = 300.0f;
	this->Top = 300.0f;
	this->Width = 720.0f;
	this->Height = 460.0f;
	this->ResizeMode = ::ResizeMode::NoResize;
	this->Background = Colors::WhiteSmoke;
	auto contentOwner = std::make_unique<Panel>();
	contentOwner->BorderThickness = 0.0f;
	contentOwner->Background = D2D1_COLOR_F{ 0, 0, 0, 0 };
	auto* contentRoot = static_cast<Panel*>(SetVisualContent(std::move(contentOwner)));
	auto addContent = [contentRoot](auto* child) { return contentRoot->AdoptVisualChild(child); };

	auto tip = addContent(cui::designer::NewControl<Label>(L"每行一个定义：auto / 数字(像素) / 50% / *(星号) / 2*", 12, 12));
	tip->Width = 690.0f;
	tip->Height = 20.0f;
	cui::designer::ApplyProgrammaticTypography(
		*tip, L"Microsoft YaHei", 12.0);

	auto rowLabel = addContent(cui::designer::NewControl<Label>(L"Rows", 12, 42));
	rowLabel->Width = 330.0f;
	rowLabel->Height = 18.0f;
	auto colLabel = addContent(cui::designer::NewControl<Label>(L"Columns", 366, 42));
	colLabel->Width = 330.0f;
	colLabel->Height = 18.0f;

	_rows = addContent(cui::designer::NewControl<RichTextBox>(L"", 12, 64, 336, 320));
	_rows->Background = Colors::White;
	_rows->BorderBrush = D2D1_COLOR_F{ 1,1,1,1 };
	_rows->Text = JoinRows(_target);

	_cols = addContent(cui::designer::NewControl<RichTextBox>(L"", 366, 64, 336, 320));
	_cols->Background = Colors::White;
	_cols->BorderBrush = D2D1_COLOR_F{ 1,1,1,1 };
	_cols->Text = JoinCols(_target);

	_ok = addContent(cui::designer::NewControl<Button>(L"确定", 12, 396, 110, 34));
	_cancel = addContent(cui::designer::NewControl<Button>(L"取消", 132, 396, 110, 34));

	_ok->Click += [this](Control*, RoutedEventArgs&) {
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
		_target->InvalidateVisual();
		this->Close();
	};

	_cancel->Click += [this](Control*, RoutedEventArgs&) {
		Applied = false;
		this->Close();
	};
}
