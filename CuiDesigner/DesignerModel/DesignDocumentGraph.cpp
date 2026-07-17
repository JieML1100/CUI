#include "DesignDocumentGraph.h"
#include <algorithm>
#include <unordered_set>

namespace DesignerModel
{
namespace
{
	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}
}

bool DesignDocumentGraph::Build(
	const DesignDocument& document,
	DesignDocumentGraph& graph,
	std::wstring* outError)
{
	graph = {};
	graph._nodes.resize(document.Nodes.size());
	graph._indexById.reserve(document.Nodes.size());
	graph._indexByName.reserve(document.Nodes.size());
	graph._childrenByParent.reserve(document.Nodes.size());

	for (size_t index = 0; index < document.Nodes.size(); ++index)
	{
		const auto& node = document.Nodes[index];
		if (node.Id < 1)
			return Fail(L"控件稳定 ID 必须为正数: " + node.Name, outError);
		if (!graph._indexById.emplace(node.Id, index).second)
			return Fail(L"控件稳定 ID 重复: " + std::to_wstring(node.Id), outError);
		if (node.Name.empty())
			return Fail(L"控件名称不能为空。", outError);
		if (!node.CustomType.Empty())
		{
			const auto& custom = node.CustomType;
			if (custom.XamlPrefix.empty() || custom.XamlName.empty()
				|| custom.XamlNamespace.empty() || custom.CppType.empty()
				|| custom.Header.empty()
				|| _wcsicmp(custom.XamlPrefix.c_str(), L"x") == 0
				|| _wcsicmp(custom.XamlPrefix.c_str(), L"d") == 0
				|| custom.XamlPrefix.find(L':') != std::wstring::npos
				|| custom.XamlName.find(L':') != std::wstring::npos
				|| node.Type == UIClass::UI_TabPage)
				return Fail(L"自定义控件类型描述不完整或包含保留名称: "
					+ node.Name, outError);
			std::wstring normalizedCppType;
			std::wstring validationError;
			if (!DesignCodeBehindModel::TryNormalizeClassName(
				custom.CppType, normalizedCppType, &validationError)
				|| normalizedCppType != custom.CppType)
				return Fail(L"自定义控件 C++ 类型必须是规范限定名称: "
					+ node.Name, outError);
			if (custom.Header.find(L"..") != std::wstring::npos
				|| custom.Header.front() == L'/'
				|| custom.Header.front() == L'\\'
				|| custom.Header.find(L':') != std::wstring::npos
				|| custom.Header.find(L'\"') != std::wstring::npos
				|| custom.Header.find_first_of(L"\r\n") != std::wstring::npos)
				return Fail(L"自定义控件头文件必须是安全的相对包含路径: "
					+ node.Name, outError);
		}
		if (!graph._indexByName.emplace(node.Name, index).second)
			return Fail(L"控件名称重复: " + node.Name, outError);
		graph._maxStableId = (std::max)(graph._maxStableId, node.Id);
		graph._nodes[index].SourceIndex = index;
	}

	if (document.NextStableId <= graph._maxStableId)
		return Fail(L"控件稳定 ID 高水位必须大于所有现有 ID。", outError);

	for (size_t index = 0; index < document.Nodes.size(); ++index)
	{
		const auto& node = document.Nodes[index];
		auto& resolved = graph._nodes[index];
		if (node.ParentId < 0)
			return Fail(L"控件 parentId 不能为负数: " + node.Name, outError);
		if (node.ParentId > 0)
		{
			const auto parent = graph._indexById.find(node.ParentId);
			if (parent == graph._indexById.end())
				return Fail(L"无法解析控件 parentId: " + node.Name, outError);
			if (node.ParentId == node.Id)
				return Fail(L"控件不能以自身作为父级: " + node.Name, outError);
			resolved.ParentKey = document.Nodes[parent->second].Name;
		}
		else
		{
			resolved.ParentKey = node.ParentRef;
			if (graph._indexByName.contains(resolved.ParentKey))
				return Fail(L"普通控件父级缺少 parentId: " + node.Name, outError);
		}
	}

	for (size_t index = 0; index < document.Nodes.size(); ++index)
	{
		std::unordered_set<int> ancestry;
		const DesignNode* current = &document.Nodes[index];
		while (current && current->ParentId > 0)
		{
			if (!ancestry.insert(current->Id).second)
				return Fail(L"检测到控件父级循环: " + document.Nodes[index].Name, outError);
			current = &document.Nodes[graph._indexById.at(current->ParentId)];
		}
	}

	for (size_t index = 0; index < graph._nodes.size(); ++index)
	{
		const auto& parentKey = graph._nodes[index].ParentKey;
		if (parentKey.empty()) graph._roots.push_back(index);
		else graph._childrenByParent[parentKey].push_back(index);
	}

	auto sortByOrder = [&](std::vector<size_t>& indices)
	{
		std::stable_sort(indices.begin(), indices.end(),
			[&](size_t left, size_t right)
			{
				return document.Nodes[left].Order < document.Nodes[right].Order;
			});
	};
	sortByOrder(graph._roots);
	for (auto& [key, children] : graph._childrenByParent)
	{
		(void)key;
		sortByOrder(children);
	}
	if (outError) outError->clear();
	return true;
}

std::span<const size_t> DesignDocumentGraph::ChildrenOf(
	const std::wstring& parentKey) const noexcept
{
	const auto found = _childrenByParent.find(parentKey);
	if (found == _childrenByParent.end()) return {};
	return found->second;
}

const ResolvedDesignNode* DesignDocumentGraph::FindById(int id) const noexcept
{
	const auto found = _indexById.find(id);
	return found == _indexById.end() ? nullptr : &_nodes[found->second];
}

const ResolvedDesignNode* DesignDocumentGraph::FindByName(
	const std::wstring& name) const noexcept
{
	const auto found = _indexByName.find(name);
	return found == _indexByName.end() ? nullptr : &_nodes[found->second];
}
}
