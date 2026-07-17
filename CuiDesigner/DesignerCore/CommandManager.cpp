#include "CommandManager.h"

DesignerDocumentTransactionResult CommandManager::Execute(
	std::unique_ptr<IDesignerCommand> command)
{
	if (!command)
	{
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"命令无效。");
	}

	const bool hadRedoHistory = !_redoStack.empty();
	const bool mayMerge = !hadRedoHistory
		&& _savedStateId != _currentStateId
		&& !_undoStack.empty();
	DesignerDocumentTransactionResult result =
		DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"命令执行失败。", false);
	try
	{
		result = command->Execute();
	}
	catch (...)
	{
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"命令执行时抛出异常。", false);
	}
	if (!result || !result.HasChanges())
	{
		return result;
	}

	if (mayMerge
		&& _undoStack.back().Command
		&& _undoStack.back().Command->TryMergeWith(*command))
	{
		auto& entry = _undoStack.back();
		if (_historyMemoryUsage >= entry.EstimatedBytes)
			_historyMemoryUsage -= entry.EstimatedBytes;
		else
			_historyMemoryUsage = 0;
		entry.EstimatedBytes = entry.Command->GetEstimatedMemoryUsage();
		_historyMemoryUsage += entry.EstimatedBytes;
		entry.AfterStateId = AllocateStateId();
		_currentStateId = entry.AfterStateId;
		TrimHistory();
		return result;
	}

	const size_t estimatedBytes = command->GetEstimatedMemoryUsage();
	try
	{
		_undoStack.reserve(_undoStack.size() + 1);
	}
	catch (...)
	{
		DesignerDocumentTransactionResult rollback =
			DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"撤销历史扩容失败，命令无法回滚。", false);
		try
		{
			rollback = command->Undo();
		}
		catch (...)
		{
			rollback = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"撤销历史扩容失败，回滚命令时抛出异常。", false);
		}
		const bool restored = rollback.Succeeded() && rollback.HasChanges();
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			restored
				? L"撤销历史内存不足，修改已回滚。"
				: L"撤销历史内存不足，修改回滚失败：" + rollback.Error,
			restored);
	}
	const auto nextStateId = AllocateStateId();
	_undoStack.push_back({
		std::move(command), _currentStateId, nextStateId, estimatedBytes
	});
	ClearRedoHistory();
	_historyMemoryUsage += estimatedBytes;
	_currentStateId = nextStateId;
	TrimHistory();
	return result;
}

bool CommandManager::CanUndo() const
{
	return !_undoStack.empty();
}

bool CommandManager::CanRedo() const
{
	return !_redoStack.empty();
}

DesignerDocumentTransactionResult CommandManager::Undo()
{
	if (_undoStack.empty())
	{
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	}

	auto entry = std::move(_undoStack.back());
	_undoStack.pop_back();
	DesignerDocumentTransactionResult result =
		DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"撤销失败。", false);
	try
	{
		result = entry.Command->Undo();
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"撤销命令时抛出异常。", false);
	}
	if (!result || !result.HasChanges())
	{
		_undoStack.push_back(std::move(entry));
		return result;
	}
	_currentStateId = entry.BeforeStateId;
	_redoStack.push_back(std::move(entry));
	return result;
}

DesignerDocumentTransactionResult CommandManager::Redo()
{
	if (_redoStack.empty())
	{
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);
	}

	auto entry = std::move(_redoStack.back());
	_redoStack.pop_back();
	DesignerDocumentTransactionResult result =
		DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"重做失败。", false);
	try
	{
		result = entry.Command->Execute();
	}
	catch (...)
	{
		result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"重做命令时抛出异常。", false);
	}
	if (!result || !result.HasChanges())
	{
		_redoStack.push_back(std::move(entry));
		return result;
	}
	_currentStateId = entry.AfterStateId;
	_undoStack.push_back(std::move(entry));
	TrimHistory();
	return result;
}

std::wstring CommandManager::GetUndoLabel() const
{
	return _undoStack.empty() || !_undoStack.back().Command
		? std::wstring{} : _undoStack.back().Command->GetLabel();
}

std::wstring CommandManager::GetRedoLabel() const
{
	return _redoStack.empty() || !_redoStack.back().Command
		? std::wstring{} : _redoStack.back().Command->GetLabel();
}

void CommandManager::Clear()
{
	_undoStack.clear();
	_redoStack.clear();
	_historyMemoryUsage = 0;
	_currentStateId = AllocateStateId();
	_savedStateId = _currentStateId;
}

void CommandManager::ResetAsUnsaved()
{
	_undoStack.clear();
	_redoStack.clear();
	_historyMemoryUsage = 0;
	_currentStateId = AllocateStateId();
	_savedStateId = AllocateStateId();
}

void CommandManager::MarkSaved()
{
	_savedStateId = _currentStateId;
}

bool CommandManager::IsDirty() const noexcept
{
	return _currentStateId != _savedStateId;
}

uint64_t CommandManager::GetCurrentStateId() const noexcept
{
	return _currentStateId;
}

uint64_t CommandManager::GetSavedStateId() const noexcept
{
	return _savedStateId;
}

void CommandManager::SetHistoryLimit(size_t historyLimit)
{
	_historyLimit = historyLimit > 0 ? historyLimit : 1;
	TrimHistory();
}

void CommandManager::SetHistoryMemoryLimit(size_t byteLimit)
{
	_historyMemoryLimit = byteLimit > 0 ? byteLimit : 1;
	TrimHistory();
}

size_t CommandManager::GetUndoCount() const noexcept
{
	return _undoStack.size();
}

size_t CommandManager::GetRedoCount() const noexcept
{
	return _redoStack.size();
}

size_t CommandManager::GetHistoryMemoryLimit() const noexcept
{
	return _historyMemoryLimit;
}

size_t CommandManager::GetHistoryMemoryUsage() const noexcept
{
	return _historyMemoryUsage;
}

void CommandManager::TrimHistory()
{
	if (_historyLimit == 0)
	{
		_historyLimit = 1;
	}

	while (_undoStack.size() > _historyLimit)
	{
		RemoveUndoEntry(0);
	}

	while (_historyMemoryUsage > _historyMemoryLimit
		&& _undoStack.size() + _redoStack.size() > 1)
	{
		if (_undoStack.size() > 1)
			RemoveUndoEntry(0);
		else if (_redoStack.size() > 1)
			RemoveRedoEntry(0);
		else if (!_redoStack.empty())
			RemoveRedoEntry(0);
		else
			break;
	}
}

void CommandManager::RemoveUndoEntry(size_t index)
{
	if (index >= _undoStack.size()) return;
	const auto bytes = _undoStack[index].EstimatedBytes;
	_undoStack.erase(_undoStack.begin() + static_cast<std::ptrdiff_t>(index));
	_historyMemoryUsage = _historyMemoryUsage >= bytes
		? _historyMemoryUsage - bytes : 0;
}

void CommandManager::RemoveRedoEntry(size_t index)
{
	if (index >= _redoStack.size()) return;
	const auto bytes = _redoStack[index].EstimatedBytes;
	_redoStack.erase(_redoStack.begin() + static_cast<std::ptrdiff_t>(index));
	_historyMemoryUsage = _historyMemoryUsage >= bytes
		? _historyMemoryUsage - bytes : 0;
}

void CommandManager::ClearRedoHistory()
{
	for (const auto& entry : _redoStack)
		_historyMemoryUsage = _historyMemoryUsage >= entry.EstimatedBytes
			? _historyMemoryUsage - entry.EstimatedBytes : 0;
	_redoStack.clear();
}

uint64_t CommandManager::AllocateStateId() noexcept
{
	if (_nextStateId == 0)
		_nextStateId = 1;
	return _nextStateId++;
}
