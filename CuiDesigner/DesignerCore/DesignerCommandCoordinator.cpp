#include "DesignerCommandCoordinator.h"
#include "../DesignerCanvas.h"
#include "Commands/DocumentSnapshotCommand.h"

namespace
{
std::wstring AppendRestoreError(
	std::wstring message,
	const std::wstring& restoreError)
{
	if (!restoreError.empty())
		message += L" 文档恢复失败：" + restoreError;
	return message;
}
}

DesignerCommandCoordinator::DesignerCommandCoordinator(DesignerCanvas* canvas)
	: _canvas(canvas)
{
}

DesignerDocumentTransactionResult DesignerCommandCoordinator::Execute(
	std::unique_ptr<IDesignerCommand> command)
{
	if (_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档事务进行中，不能执行独立命令。");
	const auto beforeStateId = _commandManager.GetCurrentStateId();
	const bool beforeDirty = _commandManager.IsDirty();
	auto result = _commandManager.Execute(std::move(command));
	if (_canvas && (beforeStateId != _commandManager.GetCurrentStateId()
		|| beforeDirty != _commandManager.IsDirty()))
		_canvas->NotifyDocumentStateChanged();
	return result;
}

DesignerDocumentTransactionResult DesignerCommandCoordinator::Undo()
{
	if (_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档事务进行中，不能撤销历史命令。");
	const auto beforeStateId = _commandManager.GetCurrentStateId();
	const bool beforeDirty = _commandManager.IsDirty();
	auto result = _commandManager.Undo();
	if (_canvas && (beforeStateId != _commandManager.GetCurrentStateId()
		|| beforeDirty != _commandManager.IsDirty()))
		_canvas->NotifyDocumentStateChanged();
	return result;
}

DesignerDocumentTransactionResult DesignerCommandCoordinator::Redo()
{
	if (_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档事务进行中，不能重做历史命令。");
	const auto beforeStateId = _commandManager.GetCurrentStateId();
	const bool beforeDirty = _commandManager.IsDirty();
	auto result = _commandManager.Redo();
	if (_canvas && (beforeStateId != _commandManager.GetCurrentStateId()
		|| beforeDirty != _commandManager.IsDirty()))
		_canvas->NotifyDocumentStateChanged();
	return result;
}

std::wstring DesignerCommandCoordinator::GetUndoLabel() const
{
	return _commandManager.GetUndoLabel();
}

std::wstring DesignerCommandCoordinator::GetRedoLabel() const
{
	return _commandManager.GetRedoLabel();
}

bool DesignerCommandCoordinator::IsDocumentDirty() const noexcept
{
	return _commandManager.IsDirty();
}

uint64_t DesignerCommandCoordinator::GetCurrentDocumentStateId() const noexcept
{
	return _commandManager.GetCurrentStateId();
}

uint64_t DesignerCommandCoordinator::GetSavedDocumentStateId() const noexcept
{
	return _commandManager.GetSavedStateId();
}

void DesignerCommandCoordinator::SetHistoryMemoryLimit(size_t byteLimit)
{
	_commandManager.SetHistoryMemoryLimit(byteLimit);
}

size_t DesignerCommandCoordinator::GetHistoryMemoryLimit() const noexcept
{
	return _commandManager.GetHistoryMemoryLimit();
}

size_t DesignerCommandCoordinator::GetHistoryMemoryUsage() const noexcept
{
	return _commandManager.GetHistoryMemoryUsage();
}

size_t DesignerCommandCoordinator::GetUndoCount() const noexcept
{
	return _commandManager.GetUndoCount();
}

size_t DesignerCommandCoordinator::GetRedoCount() const noexcept
{
	return _commandManager.GetRedoCount();
}

bool DesignerCommandCoordinator::HasActiveDocumentTransaction() const noexcept
{
	return static_cast<bool>(_interactionSnapshot.Document);
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::MarkDocumentSaved()
{
	if (_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档事务进行中，不能更新保存点。");
	const bool beforeDirty = _commandManager.IsDirty();
	_commandManager.MarkSaved();
	if (_canvas && beforeDirty != _commandManager.IsDirty())
		_canvas->NotifyDocumentStateChanged();
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Unchanged);
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::ResetHistoryAsSaved()
{
	if (_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档事务进行中，不能重置命令历史。");
	_commandManager.Clear();
	if (_canvas) _canvas->NotifyDocumentStateChanged();
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Unchanged);
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::ResetHistoryAsUnsaved()
{
	if (_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档事务进行中，不能重置命令历史。");
	_commandManager.ResetAsUnsaved();
	if (_canvas) _canvas->NotifyDocumentStateChanged();
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Unchanged);
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::ExecuteDocumentTransaction(
	const std::wstring& label,
	const DocumentEditOperation& applyChange)
{
	if (!applyChange)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"文档修改操作无效。");

	auto begin = BeginDocumentTransaction(label);
	if (!begin) return begin;

	std::wstring applyError;
	bool applied = false;
	try
	{
		applied = applyChange(applyError);
	}
	catch (...)
	{
		auto cleanup = CancelDocumentTransaction();
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			AppendRestoreError(
				L"文档修改操作抛出异常，修改已回滚。",
				cleanup.Succeeded() ? std::wstring{} : cleanup.Error),
			cleanup.Succeeded());
	}

	if (!applied)
	{
		if (applyError.empty()) applyError = L"文档修改被拒绝。";
		auto cleanup = CancelDocumentTransaction();
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Aborted,
			AppendRestoreError(
				std::move(applyError),
				cleanup.Succeeded() ? std::wstring{} : cleanup.Error),
			cleanup.Succeeded());
	}

	return CommitDocumentTransaction();
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::BeginDocumentTransaction(
	const std::wstring& label)
{
	return CaptureInteractionSnapshot(label);
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::CommitDocumentTransaction()
{
	return CommitCapturedSnapshot();
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::RollbackDocumentTransaction()
{
	return RollbackCapturedSnapshot();
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::CancelDocumentTransaction()
{
	return CancelCapturedSnapshot();
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::CaptureInteractionSnapshot(
	const std::wstring& label)
{
	if (!_canvas)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用。", false);
	if (_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"已有文档事务正在进行，不能启动嵌套事务。");

	auto document = std::make_unique<DesignerModel::DesignDocument>();
	std::wstring error;
	if (!_canvas->BuildDesignDocument(*document, &error))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立文档事务的初始快照：" + error);

	_interactionSnapshot.Document = std::move(document);
	_interactionSnapshot.SelectionNames = _canvas->CaptureSelectionNames();
	_interactionSnapshot.PrimarySelectionName = _canvas->_selectedControl ? _canvas->_selectedControl->Name : std::wstring();
	_interactionSnapshot.Label = label.empty() ? L"EditDocument" : label;
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Begun);
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::CommitCapturedSnapshot()
{
	if (!_canvas)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用。", false);
	if (!_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"没有可提交的文档事务。");

	DesignerModel::DesignDocument beforeDocument = std::move(*_interactionSnapshot.Document);
	auto beforeSelectionNames = std::move(_interactionSnapshot.SelectionNames);
	std::wstring beforeSelectionName = std::move(_interactionSnapshot.PrimarySelectionName);
	std::wstring label = std::move(_interactionSnapshot.Label);
	ClearInteractionSnapshot();

	DesignerModel::DesignDocument afterDocument;
	std::wstring error;
	if (!_canvas->BuildDesignDocument(afterDocument, &error))
	{
		std::wstring restoreError;
		const bool restored = RestoreDocumentAndSelection(
			beforeDocument, beforeSelectionNames,
			beforeSelectionName, restoreError);
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			AppendRestoreError(
				L"修改会产生无效设计文档，已回滚：" + error,
				restoreError),
			restored);
	}

	std::wstring afterSelectionName = _canvas->_selectedControl ? _canvas->_selectedControl->Name : std::wstring();
	auto afterSelectionNames = _canvas->CaptureSelectionNames();
	if (beforeDocument == afterDocument && beforeSelectionName == afterSelectionName && beforeSelectionNames == afterSelectionNames)
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);

	auto rollbackDocument = beforeDocument;
	auto rollbackSelectionNames = beforeSelectionNames;
	auto rollbackSelectionName = beforeSelectionName;

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
	auto commandResult = Execute(std::move(command));
	if (commandResult.HasChanges())
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Committed);

	std::wstring restoreError;
	const bool restored = RestoreDocumentAndSelection(
		rollbackDocument, rollbackSelectionNames,
		rollbackSelectionName, restoreError);
	return DesignerDocumentTransactionResult::Failure(
		DesignerDocumentTransactionState::Failed,
		AppendRestoreError(
			commandResult.Error.empty()
				? L"无法把修改加入撤销栈，已回滚。"
				: L"无法把修改加入撤销栈，已回滚："
					+ commandResult.Error,
			restoreError),
		restored);
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::RollbackCapturedSnapshot()
{
	if (!_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"没有可回滚的文档事务。");
	if (!_canvas)
	{
		ClearInteractionSnapshot();
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用，文档无法恢复。", false);
	}

	DesignerModel::DesignDocument document =
		std::move(*_interactionSnapshot.Document);
	auto selectionNames = std::move(_interactionSnapshot.SelectionNames);
	auto primarySelectionName =
		std::move(_interactionSnapshot.PrimarySelectionName);
	ClearInteractionSnapshot();

	std::wstring error;
	const bool restored = RestoreDocumentAndSelection(
		document, selectionNames, primarySelectionName, error);
	if (!restored)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"文档回滚失败：" + error, false);
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::RolledBack);
}

DesignerDocumentTransactionResult
DesignerCommandCoordinator::CancelCapturedSnapshot()
{
	if (!_interactionSnapshot.Document)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"没有可取消的文档事务。");
	if (!_canvas)
	{
		ClearInteractionSnapshot();
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用，无法验证取消操作。", false);
	}

	DesignerModel::DesignDocument currentDocument;
	std::wstring error;
	if (!_canvas->BuildDesignDocument(currentDocument, &error))
		return RollbackCapturedSnapshot();

	const auto currentSelectionNames = _canvas->CaptureSelectionNames();
	const auto currentPrimarySelectionName = _canvas->_selectedControl
		? _canvas->_selectedControl->Name : std::wstring();
	if (*_interactionSnapshot.Document == currentDocument
		&& _interactionSnapshot.SelectionNames == currentSelectionNames
		&& _interactionSnapshot.PrimarySelectionName
			== currentPrimarySelectionName)
	{
		ClearInteractionSnapshot();
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Canceled);
	}

	return RollbackCapturedSnapshot();
}

bool DesignerCommandCoordinator::RestoreDocumentAndSelection(
	const DesignerModel::DesignDocument& document,
	const std::vector<std::wstring>& selectionNames,
	const std::wstring& primarySelectionName,
	std::wstring& error) const
{
	if (!_canvas)
	{
		error = L"设计画布不可用。";
		return false;
	}
	if (!_canvas->ApplyDesignDocument(document, &error)) return false;
	_canvas->RestoreSelectionByNames(
		selectionNames, primarySelectionName, true);
	return true;
}

void DesignerCommandCoordinator::ClearInteractionSnapshot()
{
	_interactionSnapshot.Document.reset();
	_interactionSnapshot.SelectionNames.clear();
	_interactionSnapshot.PrimarySelectionName.clear();
	_interactionSnapshot.Label.clear();
}
