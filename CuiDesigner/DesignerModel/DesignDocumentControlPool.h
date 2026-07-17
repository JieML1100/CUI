#pragma once

#include "DesignDocumentGraph.h"
#include "../../CUI/include/Control.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace DesignerModel
{
/**
 * Owns newly instantiated document controls until a materializer attaches
 * them to their runtime parents. Destruction is rollback: every control that
 * was not taken is released safely.
 */
class DesignDocumentControlPool
{
public:
	using Factory = std::function<std::unique_ptr<Control>(const DesignNode&)>;

	static bool Build(
		const DesignDocument& document,
		const DesignDocumentGraph& graph,
		const Factory& factory,
		DesignDocumentControlPool& pool,
		std::wstring* outError = nullptr);

	Control* FindById(int id) const noexcept;
	Control* FindByName(const std::wstring& name) const noexcept;
	std::unique_ptr<Control> TakeById(int id) noexcept;
	std::unique_ptr<Control> TakeByName(
		const std::wstring& name) noexcept;
	size_t PendingCount() const noexcept;

private:
	struct Entry
	{
		int Id = 0;
		std::wstring Name;
		std::unique_ptr<Control> Owner;
		Control* Instance = nullptr;
	};

	std::vector<Entry> _entries;
	std::unordered_map<int, size_t> _indexById;
	std::unordered_map<std::wstring, size_t> _indexByName;
};
}
