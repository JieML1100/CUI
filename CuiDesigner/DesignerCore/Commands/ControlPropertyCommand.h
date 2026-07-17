#pragma once

#include "../CommandManager.h"
#include "../../DesignerPropertyEdit.h"
#include <chrono>
#include <string>
#include <vector>

class DesignerCanvas;

/** Reversible per-property delta; it never owns a full DesignDocument. */
class ControlPropertyCommand final : public IDesignerCommand
{
public:
	ControlPropertyCommand(
		DesignerCanvas* canvas,
		DesignerPropertyBatchSnapshot before,
		DesignerPropertyBatchSnapshot after,
		std::vector<std::wstring> beforeSelectionNames,
		std::vector<std::wstring> afterSelectionNames,
		std::wstring beforePrimarySelectionName,
		std::wstring afterPrimarySelectionName,
		std::wstring label,
		bool skipInitialExecute,
		bool allowMerge = true);

	DesignerDocumentTransactionResult Execute() override;
	DesignerDocumentTransactionResult Undo() override;
	std::wstring GetLabel() const override;
	bool TryMergeWith(IDesignerCommand& newer) noexcept override;
	size_t GetEstimatedMemoryUsage() const noexcept override;

private:
	DesignerDocumentTransactionResult Apply(
		const DesignerPropertyBatchSnapshot& expected,
		const DesignerPropertyBatchSnapshot& desired,
		const std::vector<std::wstring>& selectionNames,
		const std::wstring& primarySelectionName) const;
	void RefreshEstimatedMemoryUsage() noexcept;

	DesignerCanvas* _canvas = nullptr;
	DesignerPropertyBatchSnapshot _before;
	DesignerPropertyBatchSnapshot _after;
	std::vector<std::wstring> _beforeSelectionNames;
	std::vector<std::wstring> _afterSelectionNames;
	std::wstring _beforePrimarySelectionName;
	std::wstring _afterPrimarySelectionName;
	std::wstring _label;
	bool _skipInitialExecute = false;
	bool _allowMerge = true;
	std::chrono::steady_clock::time_point _committedAt;
	size_t _estimatedMemoryUsage = 0;
};
