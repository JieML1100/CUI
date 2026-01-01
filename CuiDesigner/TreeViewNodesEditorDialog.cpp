#include "TreeViewNodesEditorDialog.h"
#include <sstream>

std::wstring TreeViewNodesEditorDialog::Trim(const std::wstring& s)
{
	size_t start = 0;
	while (start < s.size() && iswspace(s[start])) start++;
	size_t end = s.size();
	while (end > start && iswspace(s[end - 1])) end--;
	return s.substr(start, end - start);
}

std::vector<std::wstring> TreeViewNodesEditorDialog::SplitLines(const std::wstring& text)
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

int TreeViewNodesEditorDialog::CountIndentTabs(const std::wstring& s)
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

std::wstring TreeViewNodesEditorDialog::StripIndentTabs(const std::wstring& s)
{
	// 移除所有前导缩进空白（tab/space）。
	size_t i = 0;
	while (i < s.size() && (s[i] == L'\t' || s[i] == L' ')) i++;
	return s.substr(i);
}

void TreeViewNodesEditorDialog::SerializeNodes(std::wstringstream& ss, List<TreeNode*>& nodes, int depth)
{
	for (auto n : nodes)
	{
		for (int i = 0; i < depth; i++) ss << L"\t";
		ss << (n ? n->Text : L"") << L"\r\n";
		if (n && n->Children.Count > 0)
			SerializeNodes(ss, n->Children, depth + 1);
	}
}

std::wstring TreeViewNodesEditorDialog::NodesToText(TreeView* tv)
{
	if (!tv || !tv->Root) return L"";
	std::wstringstream ss;
	SerializeNodes(ss, tv->Root->Children, 0);
	return ss.str();
}

TreeViewNodesEditorDialog::TreeViewNodesEditorDialog(TreeView* target)
	: Form(L"编辑 TreeView 节点", POINT{ 280, 280 }, SIZE{ 600, 460 }), _target(target)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->BackColor = Colors::WhiteSmoke;
	this->AllowResize = false;

	auto tip = this->AddControl(new Label(L"用 \t 缩进表示层级：\n一级\n\t二级\n\t\t三级", 12, 12));
	tip->Size = { 570, 44 };
	tip->Font = new ::Font(L"Microsoft YaHei", 12.0f);

	_editor = this->AddControl(new RichTextBox(L"", 12, 60, 576, 330));
	_editor->AllowMultiLine = true;
	_editor->AllowTabInput = true;
	_editor->BackColor = Colors::White;
	_editor->FocusedColor = D2D1_COLOR_F{ 1,1,1,1 };
	_editor->Text = NodesToText(_target);

	_ok = this->AddControl(new Button(L"确定", 12, 402, 110, 34));
	_cancel = this->AddControl(new Button(L"取消", 132, 402, 110, 34));

	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		if (!_target || !_target->Root || !_editor) { this->Close(); return; }

		auto lines = SplitLines(_editor->Text);

		// 清空旧节点
		for (auto n : _target->Root->Children) delete n;
		_target->Root->Children.Clear();
		_target->SelectedNode = nullptr;
		_target->HoveredNode = nullptr;

		std::vector<TreeNode*> stack;
		std::vector<int> depthStack;

		for (auto& raw : lines)
		{
			if (raw.empty()) continue;
			int depth = CountIndentTabs(raw);
			auto text = Trim(StripIndentTabs(raw));
			if (text.empty()) continue;

			auto* node = new TreeNode(text);
			if (depth <= 0)
			{
				_target->Root->Children.push_back(node);
				stack.clear();
				depthStack.clear();
				stack.push_back(node);
				depthStack.push_back(0);
			}
			else
			{
				// 找到最近的父层
				while (!depthStack.empty() && depthStack.back() >= depth)
				{
					depthStack.pop_back();
					stack.pop_back();
				}
				TreeNode* parent = stack.empty() ? nullptr : stack.back();
				if (!parent)
				{
					_target->Root->Children.push_back(node);
					stack.clear();
					depthStack.clear();
					stack.push_back(node);
					depthStack.push_back(0);
				}
				else
				{
					parent->Children.push_back(node);
					parent->Expand = true;
					stack.push_back(node);
					depthStack.push_back(depth);
				}
			}
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
