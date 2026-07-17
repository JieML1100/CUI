#pragma once

#include "DesignDocument.h"
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace DesignerModel
{
struct ResolvedDesignNode
{
	size_t SourceIndex = 0;
	// Empty means form root. Ordinary parents are canonicalized from ParentId;
	// synthetic parents such as TabPage retain their persisted ParentRef key.
	std::wstring ParentKey;
};

/**
 * Validated, deterministic topology view over a DesignDocument.
 *
 * It owns only indices and keys; the source document must outlive the graph.
 * Both static code generation and dynamic materialization use this layer so
 * identity, parent resolution and child ordering cannot drift apart.
 */
class DesignDocumentGraph
{
public:
	static bool Build(
		const DesignDocument& document,
		DesignDocumentGraph& graph,
		std::wstring* outError = nullptr);

	std::span<const ResolvedDesignNode> Nodes() const noexcept
	{
		return _nodes;
	}
	std::span<const size_t> Roots() const noexcept
	{
		return _roots;
	}
	std::span<const size_t> ChildrenOf(
		const std::wstring& parentKey) const noexcept;
	const ResolvedDesignNode* FindById(int id) const noexcept;
	const ResolvedDesignNode* FindByName(
		const std::wstring& name) const noexcept;
	int MaxStableId() const noexcept { return _maxStableId; }

private:
	std::vector<ResolvedDesignNode> _nodes;
	std::vector<size_t> _roots;
	std::unordered_map<std::wstring, std::vector<size_t>> _childrenByParent;
	std::unordered_map<int, size_t> _indexById;
	std::unordered_map<std::wstring, size_t> _indexByName;
	int _maxStableId = 0;
};
}
