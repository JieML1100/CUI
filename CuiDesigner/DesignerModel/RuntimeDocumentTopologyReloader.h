#pragma once

#include "RuntimeDocument.h"

#include <cstddef>
#include <functional>
#include <string>

namespace DesignerModel
{
/** Internal transactional topology recomposition used by RuntimeDocumentLoader. */
class RuntimeDocumentTopologyReloader final
{
public:
	using CandidateCommit = std::function<bool(
		RuntimeDocument& candidate,
		std::wstring* outError)>;

	static bool TryReload(
		const DesignDocument& document,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& effectiveOptions,
		bool& outApplied,
		size_t& outReusedControlCount,
		std::wstring* outError = nullptr,
		const CandidateCommit& candidateCommit = {});
};
}
