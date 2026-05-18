#include "DesignerCommandCoordinator.h"
#include "../DesignerCanvas.h"
#include "Commands/AddDeleteCommand.h"
#include "Commands/DocumentSnapshotCommand.h"

DesignerCommandCoordinator::DesignerCommandCoordinator(DesignerCanvas* canvas)
	: _canvas(canvas)
{
}

bool DesignerCommandCoordinator::Execute(std::unique_ptr<IDesignerCommand> command)
{
	return _commandManager.Execute(std::move(command));
}

bool DesignerCommandCoordinator::Undo()
{
	return _commandManager.Undo();
}

bool DesignerCommandCoordinator::Redo()
{
	return _commandManager.Redo();
}

void DesignerCommandCoordinator::ExecuteDocumentSnapshotCommand(const std::wstring& label, const std::function<void()>& applyChange)
{
	if (!_canvas || !applyChange)
	{
		return;
	}

	DesignerModel::DesignDocument beforeDocument;
	std::wstring error;
	if (!_canvas->BuildDesignDocument(beforeDocument, &error))
	{
		applyChange();
		return;
	}

	std::wstring beforeSelectionName;
	if (_canvas->_selectedControl)
	{
		beforeSelectionName = _canvas->_selectedControl->Name;
	}
	auto beforeSelectionNames = _canvas->CaptureSelectionNames();

	applyChange();

	DesignerModel::DesignDocument afterDocument;
	if (!_canvas->BuildDesignDocument(afterDocument, &error))
	{
		return;
	}

	std::wstring afterSelectionName;
	if (_canvas->_selectedControl)
	{
		afterSelectionName = _canvas->_selectedControl->Name;
	}
	auto afterSelectionNames = _canvas->CaptureSelectionNames();

	if (beforeDocument == afterDocument && beforeSelectionName == afterSelectionName && beforeSelectionNames == afterSelectionNames)
	{
		return;
	}

	auto command = std::make_unique<AddDeleteCommand>(
		_canvas,
		std::move(beforeDocument),
		std::move(afterDocument),
		std::move(beforeSelectionNames),
		std::move(afterSelectionNames),
		std::move(beforeSelectionName),
		std::move(afterSelectionName),
		label,
		true);
	Execute(std::move(command));
}

void DesignerCommandCoordinator::BeginInteractionSnapshot(const std::wstring& label)
{
	if (!_canvas || _interactionSnapshot.Document)
	{
		return;
	}

	auto document = std::make_unique<DesignerModel::DesignDocument>();
	std::wstring error;
	if (!_canvas->BuildDesignDocument(*document, &error))
	{
		return;
	}

	_interactionSnapshot.Document = std::move(document);
	_interactionSnapshot.SelectionNames = _canvas->CaptureSelectionNames();
	_interactionSnapshot.PrimarySelectionName = _canvas->_selectedControl ? _canvas->_selectedControl->Name : std::wstring();
	_interactionSnapshot.Label = label;
}

void DesignerCommandCoordinator::CommitInteractionSnapshot()
{
	if (!_canvas || !_interactionSnapshot.Document)
	{
		return;
	}

	DesignerModel::DesignDocument beforeDocument = std::move(*_interactionSnapshot.Document);
	auto beforeSelectionNames = std::move(_interactionSnapshot.SelectionNames);
	std::wstring beforeSelectionName = std::move(_interactionSnapshot.PrimarySelectionName);
	std::wstring label = std::move(_interactionSnapshot.Label);
	ClearInteractionSnapshot();

	DesignerModel::DesignDocument afterDocument;
	std::wstring error;
	if (!_canvas->BuildDesignDocument(afterDocument, &error))
	{
		return;
	}

	std::wstring afterSelectionName = _canvas->_selectedControl ? _canvas->_selectedControl->Name : std::wstring();
	auto afterSelectionNames = _canvas->CaptureSelectionNames();
	if (beforeDocument == afterDocument && beforeSelectionName == afterSelectionName && beforeSelectionNames == afterSelectionNames)
	{
		return;
	}

	auto command = std::make_unique<DocumentSnapshotCommand>(
		_canvas,
		std::move(beforeDocument),
		std::move(afterDocument),
		std::move(beforeSelectionNames),
		std::move(afterSelectionNames),
		std::move(beforeSelectionName),
		std::move(afterSelectionName),
		std::move(label),
		true);
	Execute(std::move(command));
}

void DesignerCommandCoordinator::ClearInteractionSnapshot()
{
	_interactionSnapshot.Document.reset();
	_interactionSnapshot.SelectionNames.clear();
	_interactionSnapshot.PrimarySelectionName.clear();
	_interactionSnapshot.Label.clear();
}