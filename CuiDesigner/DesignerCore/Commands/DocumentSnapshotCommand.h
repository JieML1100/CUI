#pragma once

#include "../CommandManager.h"
#include "../../DesignerModel/DesignDocument.h"
#include <string>
#include <vector>

class DesignerCanvas;

class DocumentSnapshotCommand : public IDesignerCommand
{
public:
	DocumentSnapshotCommand(
		DesignerCanvas* canvas,
		DesignerModel::DesignDocument beforeDocument,
		DesignerModel::DesignDocument afterDocument,
		std::vector<std::wstring> beforeSelectionNames,
		std::vector<std::wstring> afterSelectionNames,
		std::wstring beforeSelectionName,
		std::wstring afterSelectionName,
		std::wstring label,
		bool skipInitialExecute);

	bool Execute() override;
	void Undo() override;
	std::wstring GetLabel() const override;

protected:
	bool Apply(const DesignerModel::DesignDocument& document, const std::vector<std::wstring>& selectionNames, const std::wstring& primarySelectionName) const;

	DesignerCanvas* _canvas = nullptr;
	DesignerModel::DesignDocument _beforeDocument;
	DesignerModel::DesignDocument _afterDocument;
	std::vector<std::wstring> _beforeSelectionNames;
	std::vector<std::wstring> _afterSelectionNames;
	std::wstring _beforeSelectionName;
	std::wstring _afterSelectionName;
	std::wstring _label;
	bool _skipInitialExecute = false;
};