#pragma once

#include "../CommandManager.h"
#include "../../DesignerStructureEdit.h"

#include <string>
#include <vector>

class DesignerCanvas;

/** Reversible one-control collection delta; never owns a DesignDocument. */
class ControlStructureCommand final : public IDesignerCommand
{
public:
	ControlStructureCommand(
		DesignerCanvas* canvas,
		DesignerStructureSnapshot before,
		DesignerStructureSnapshot after,
		std::vector<std::wstring> beforeSelectionNames,
		std::vector<std::wstring> afterSelectionNames,
		std::wstring beforePrimarySelectionName,
		std::wstring afterPrimarySelectionName,
		std::wstring label,
		bool skipInitialExecute);

	DesignerDocumentTransactionResult Execute() override;
	DesignerDocumentTransactionResult Undo() override;
	std::wstring GetLabel() const override;
	size_t GetEstimatedMemoryUsage() const noexcept override;

private:
	DesignerDocumentTransactionResult Apply(
		const DesignerStructureSnapshot& expected,
		const DesignerStructureSnapshot& desired,
		const std::vector<std::wstring>& selectionNames,
		const std::wstring& primarySelectionName) const;

	DesignerCanvas* _canvas = nullptr;
	DesignerStructureSnapshot _before;
	DesignerStructureSnapshot _after;
	std::vector<std::wstring> _beforeSelectionNames;
	std::vector<std::wstring> _afterSelectionNames;
	std::wstring _beforePrimarySelectionName;
	std::wstring _afterPrimarySelectionName;
	std::wstring _label;
	bool _skipInitialExecute = false;
	size_t _estimatedMemoryUsage = 0;
};
