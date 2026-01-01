#include "MenuItemsEditorDialog.h"
#include <algorithm>
#include <sstream>

std::wstring MenuItemsEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

std::vector<std::wstring> MenuItemsEditorDialog::SplitLines(const std::wstring& text)
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

int MenuItemsEditorDialog::CountIndentTabs(const std::wstring& s)
{
	// 兼容：允许使用 tab 或空格缩进（4 个空格视为 1 级）。
	int depth = 0;
	int spaceRun = 0;
	for (size_t i = 0; i < s.size(); i++)
	{
		wchar_t c = s[i];
		if (c == L'\t')
		{
			depth++;
			spaceRun = 0;
			continue;
		}
		if (c == L' ')
		{
			spaceRun++;
			if (spaceRun >= 4)
			{
				depth++;
				spaceRun = 0;
			}
			continue;
		}
		break;
	}
	return depth;
}

std::wstring MenuItemsEditorDialog::StripIndentTabs(const std::wstring& s)
{
	size_t i = 0;
	while (i < s.size() && (s[i] == L'\t' || s[i] == L' ')) i++;
	return s.substr(i);
}

bool MenuItemsEditorDialog::ParseBool(const std::wstring& s, bool def)
{
	auto t = Trim(s);
	if (t.empty()) return def;
	std::wstring low = t;
	std::transform(low.begin(), low.end(), low.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
	if (low == L"1" || low == L"true" || low == L"yes" || low == L"y") return true;
	if (low == L"0" || low == L"false" || low == L"no" || low == L"n") return false;
	return def;
}

int MenuItemsEditorDialog::ParseInt(const std::wstring& s, int def)
{
	auto t = Trim(s);
	if (t.empty()) return def;
	try { return std::stoi(t); }
	catch (...) { return def; }
}

std::vector<std::wstring> MenuItemsEditorDialog::SplitByPipe(const std::wstring& s)
{
	std::vector<std::wstring> parts;
	std::wstring cur;
	for (auto c : s)
	{
		if (c == L'|')
		{
			parts.push_back(cur);
			cur.clear();
			continue;
		}
		cur.push_back(c);
	}
	parts.push_back(cur);
	return parts;
}

MenuItemsEditorDialog::ItemModel MenuItemsEditorDialog::FromMenuItem(MenuItem* it)
{
	ItemModel m;
	if (!it) return m;
	m.Text = it->Text;
	m.Id = it->Id;
	m.Shortcut = it->Shortcut;
	m.Separator = it->Separator;
	m.Enable = it->Enable;
	m.SubItems.reserve(it->SubItems.size());
	for (auto* si : it->SubItems)
	{
		if (!si) continue;
		m.SubItems.push_back(FromMenuItem(si));
	}
	return m;
}

void MenuItemsEditorDialog::LoadModelFromTarget()
{
	_tops.clear();
	if (!_target) return;

	for (int i = 0; i < _target->Count; i++)
	{
		auto* top = dynamic_cast<MenuItem*>(_target->operator[](i));
		if (!top) continue;
		_tops.push_back(FromMenuItem(top));
	}
}

void MenuItemsEditorDialog::SerializeItems(std::wstringstream& ss, const std::vector<ItemModel>& items, int depth)
{
	for (auto& it : items)
	{
		for (int i = 0; i < depth; i++) ss << L"\t";
		if (it.Separator)
		{
			ss << L"----\r\n";
			continue;
		}

		auto text = Trim(it.Text);
		if (text.empty()) continue;
		ss << text;

		bool needExtras = (it.Id != 0) || !it.Shortcut.empty() || !it.Enable;
		if (needExtras)
		{
			ss << L" | " << it.Id << L" | " << it.Shortcut << L" | " << (it.Enable ? L"true" : L"false");
		}
		ss << L"\r\n";

		if (!it.SubItems.empty())
			SerializeItems(ss, it.SubItems, depth + 1);
	}
}

std::wstring MenuItemsEditorDialog::ModelToText(const std::vector<ItemModel>& tops)
{
	std::wstringstream ss;
	SerializeItems(ss, tops, 0);
	return ss.str();
}

std::vector<MenuItemsEditorDialog::ItemModel> MenuItemsEditorDialog::TextToModel(const std::wstring& text)
{
	auto lines = SplitLines(text);
	std::vector<ItemModel> tops;
	std::vector<ItemModel*> stack;
	std::vector<int> depthStack;

	auto isDashSeparator = [&](const std::wstring& s) -> bool
		{
			if (s.size() < 2) return false;
			for (auto c : s)
				if (c != L'-') return false;
			return true;
		};

	for (auto& raw : lines)
	{
		int depth = CountIndentTabs(raw);
		auto stripped = StripIndentTabs(raw);
		auto trimmed = Trim(stripped);

		ItemModel m;
		if (trimmed.empty())
		{
			m.Separator = true;
			m.Enable = true;
		}
		else
		{
			auto parts = SplitByPipe(trimmed);
			auto head = Trim(parts.size() > 0 ? parts[0] : L"");
			if (isDashSeparator(head))
			{
				m.Separator = true;
				m.Enable = true;
			}
			else
			{
				m.Text = head;
			m.Id = ParseInt(parts.size() > 1 ? parts[1] : L"", 0);
			m.Shortcut = Trim(parts.size() > 2 ? parts[2] : L"");
			m.Enable = ParseBool(parts.size() > 3 ? parts[3] : L"", true);
				m.Separator = false;
				if (m.Text.empty())
					continue;
			}
		}

		if (depth <= 0)
		{
			tops.push_back(std::move(m));
			stack.clear();
			depthStack.clear();
			if (!tops.back().Separator)
			{
				stack.push_back(&tops.back());
				depthStack.push_back(0);
			}
			continue;
		}

		while (!depthStack.empty() && depthStack.back() >= depth)
		{
			depthStack.pop_back();
			stack.pop_back();
		}

		ItemModel* parent = stack.empty() ? nullptr : stack.back();
		if (!parent)
		{
			// 没有可用父节点，降级为顶层。
			if (!m.Separator)
			{
				tops.push_back(std::move(m));
				stack.clear();
				depthStack.clear();
				stack.push_back(&tops.back());
				depthStack.push_back(0);
			}
			continue;
		}

		parent->SubItems.push_back(std::move(m));
		auto* added = &parent->SubItems.back();
		if (!added->Separator)
		{
			stack.push_back(added);
			depthStack.push_back(depth);
		}
	}

	return tops;
}

void MenuItemsEditorDialog::ApplySubItems(MenuItem* parent, const std::vector<ItemModel>& subs)
{
	if (!parent) return;
	for (auto& s : subs)
	{
		if (s.Separator)
		{
			parent->AddSeparator();
			continue;
		}
		auto st = Trim(s.Text);
		if (st.empty()) continue;
		auto* si = parent->AddSubItem(st, s.Id);
		if (!si) continue;
		si->Shortcut = s.Shortcut;
		si->Enable = s.Enable;
		if (!s.SubItems.empty())
			ApplySubItems(si, s.SubItems);
	}
}

void MenuItemsEditorDialog::ApplyToTarget()
{
	if (!_target) return;

	if (_editor)
		_tops = TextToModel(_editor->Text);

	// 清空现有顶层 MenuItem（会递归释放 SubItems）
	while (_target->Count > 0)
	{
		auto* c = _target->operator[](_target->Count - 1);
		_target->RemoveControl(c);
		delete c;
	}

	for (auto& t : _tops)
	{
		if (t.Separator)
		{
			// 顶层也允许分割线（表现为不可交互的分隔项）。
			auto* sep = _target->AddControl(MenuItem::CreateSeparator());
			if (sep) sep->Height = _target->BarHeight;
			continue;
		}

		auto text = Trim(t.Text);
		if (text.empty()) continue;
		auto* top = _target->AddItem(text);
		if (!top) continue;
		top->Id = t.Id;
		top->Shortcut = t.Shortcut;
		top->Enable = t.Enable;
		if (!t.SubItems.empty())
			ApplySubItems(top, t.SubItems);
	}

	_target->PostRender();
}

MenuItemsEditorDialog::MenuItemsEditorDialog(Menu* target)
	: Form(L"编辑 Menu 菜单项", POINT{ 280, 280 }, SIZE{ 640, 480 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto tip = this->AddControl(new Label(
		L"用 \t 缩进表示层级（支持多级子菜单）；用 ---- 表示分割线（顶层/子层都支持）。\n"
		L"可选：行内字段 Text | Id | Shortcut | Enable（不填则使用默认值）",
		12, 12));
	tip->Size = { 616, 44 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_editor = this->AddControl(new RichTextBox(L"", 12, 60, 616, 340));
	_editor->AllowMultiLine = true;
	_editor->AllowTabInput = true;
	_editor->BackColor = Colors::White;
	_editor->FocusedColor = D2D1_COLOR_F{ 1,1,1,1 };

	_ok = this->AddControl(new Button(L"确定", 12, 414, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 414, 110, 34));

	LoadModelFromTarget();
	_editor->Text = ModelToText(_tops);

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		ApplyToTarget();
		Applied = true;
		this->Close();
	};
	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
	};
}
