#include "DocumentSnapshotCommand.h"
#include "../../DesignerCanvas.h"

DocumentSnapshotCommand::DocumentSnapshotCommand(
	DesignerCanvas* canvas,
	DesignerModel::DesignDocument beforeDocument,
	DesignerModel::DesignDocument afterDocument,
	std::vector<std::wstring> beforeSelectionNames,
	std::vector<std::wstring> afterSelectionNames,
	std::wstring beforeSelectionName,
	std::wstring afterSelectionName,
	std::wstring label,
	bool skipInitialExecute)
	: _canvas(canvas),
	  _beforeDocument(std::move(beforeDocument)),
	  _afterDocument(std::move(afterDocument)),
	  _beforeSelectionNames(std::move(beforeSelectionNames)),
	  _afterSelectionNames(std::move(afterSelectionNames)),
	  _beforeSelectionName(std::move(beforeSelectionName)),
	  _afterSelectionName(std::move(afterSelectionName)),
	  _label(std::move(label)),
	  _skipInitialExecute(skipInitialExecute)
{
}

bool DocumentSnapshotCommand::Execute()
{
	if (_skipInitialExecute)
	{
		_skipInitialExecute = false;
		return true;
	}

	return Apply(_afterDocument, _afterSelectionNames, _afterSelectionName);
}

void DocumentSnapshotCommand::Undo()
{
	Apply(_beforeDocument, _beforeSelectionNames, _beforeSelectionName);
}

std::wstring DocumentSnapshotCommand::GetLabel() const
{
	return _label;
}

bool DocumentSnapshotCommand::Apply(const DesignerModel::DesignDocument& document, const std::vector<std::wstring>& selectionNames, const std::wstring& primarySelectionName) const
{
	if (!_canvas)
	{
		return false;
	}

	std::wstring error;
	if (!_canvas->ApplyDesignDocument(document, &error))
	{
		return false;
	}
	_canvas->RestoreSelectionByNames(selectionNames, primarySelectionName, true);
	return true;
}