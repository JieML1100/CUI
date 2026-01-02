#pragma once

/**
 * @file TreeViewNodesEditorDialog.h
 * @brief TreeViewNodesEditorDialog：编辑 TreeView 节点树的对话框。
 */
#include "../CUI/GUI/Form.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/RichTextBox.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/TreeView.h"

class TreeViewNodesEditorDialog : public Form
{
public:
	bool Applied = false;

	TreeViewNodesEditorDialog(TreeView* target);
	~TreeViewNodesEditorDialog() = default;

private:
	TreeView* _target = nullptr;
	RichTextBox* _editor = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;

	static std::wstring Trim(const std::wstring& s);
	static std::vector<std::wstring> SplitLines(const std::wstring& text);
	static int CountIndentTabs(const std::wstring& s);
	static std::wstring StripIndentTabs(const std::wstring& s);
	static void SerializeNodes(std::wstringstream& ss, List<TreeNode*>& nodes, int depth);
	static std::wstring NodesToText(TreeView* tv);
};
