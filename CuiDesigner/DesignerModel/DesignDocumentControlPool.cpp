#include "DesignDocumentControlPool.h"

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

bool DesignDocumentControlPool::Build(
	const DesignDocument& document,
	const DesignDocumentGraph& graph,
	const Factory& factory,
	DesignDocumentControlPool& pool,
	std::wstring* outError)
{
	pool = {};
	if (!factory)
		return Fail(L"控件工厂不可用。", outError);
	pool._entries.reserve(graph.Nodes().size());
	pool._indexById.reserve(graph.Nodes().size());
	pool._indexByName.reserve(graph.Nodes().size());

	for (const auto& resolved : graph.Nodes())
	{
		const auto& node = document.Nodes[resolved.SourceIndex];
		auto owner = factory(node);
		if (!owner)
			return Fail(L"无法创建控件实例: " + node.Name, outError);
		owner->DesignId = node.Id;
		Entry entry;
		entry.Id = node.Id;
		entry.Name = node.Name;
		entry.Instance = owner.get();
		entry.Owner = std::move(owner);
		const size_t index = pool._entries.size();
		pool._entries.push_back(std::move(entry));
		pool._indexById.emplace(node.Id, index);
		pool._indexByName.emplace(node.Name, index);
	}
	if (outError) outError->clear();
	return true;
}

Control* DesignDocumentControlPool::FindById(int id) const noexcept
{
	const auto found = _indexById.find(id);
	return found == _indexById.end()
		? nullptr : _entries[found->second].Instance;
}

Control* DesignDocumentControlPool::FindByName(
	const std::wstring& name) const noexcept
{
	const auto found = _indexByName.find(name);
	return found == _indexByName.end()
		? nullptr : _entries[found->second].Instance;
}

std::unique_ptr<Control> DesignDocumentControlPool::TakeById(int id) noexcept
{
	const auto found = _indexById.find(id);
	if (found == _indexById.end()) return {};
	return std::move(_entries[found->second].Owner);
}

std::unique_ptr<Control> DesignDocumentControlPool::TakeByName(
	const std::wstring& name) noexcept
{
	const auto found = _indexByName.find(name);
	if (found == _indexByName.end()) return {};
	return std::move(_entries[found->second].Owner);
}

size_t DesignDocumentControlPool::PendingCount() const noexcept
{
	size_t result = 0;
	for (const auto& entry : _entries)
		if (entry.Owner) ++result;
	return result;
}
}
