#include "CommandManager.h"

bool CommandManager::Execute(std::unique_ptr<IDesignerCommand> command)
{
	if (!command)
	{
		return false;
	}

	if (!command->Execute())
	{
		return false;
	}

	_redoStack.clear();
	_undoStack.push_back(std::move(command));
	TrimUndoHistory();
	return true;
}

bool CommandManager::CanUndo() const
{
	return !_undoStack.empty();
}

bool CommandManager::CanRedo() const
{
	return !_redoStack.empty();
}

bool CommandManager::Undo()
{
	if (_undoStack.empty())
	{
		return false;
	}

	auto command = std::move(_undoStack.back());
	_undoStack.pop_back();
	command->Undo();
	_redoStack.push_back(std::move(command));
	return true;
}

bool CommandManager::Redo()
{
	if (_redoStack.empty())
	{
		return false;
	}

	auto command = std::move(_redoStack.back());
	_redoStack.pop_back();
	if (!command->Execute())
	{
		_redoStack.push_back(std::move(command));
		return false;
	}
	_undoStack.push_back(std::move(command));
	TrimUndoHistory();
	return true;
}

void CommandManager::Clear()
{
	_undoStack.clear();
	_redoStack.clear();
}

void CommandManager::SetHistoryLimit(size_t historyLimit)
{
	_historyLimit = historyLimit > 0 ? historyLimit : 1;
	TrimUndoHistory();
}

size_t CommandManager::GetUndoCount() const
{
	return _undoStack.size();
}

size_t CommandManager::GetRedoCount() const
{
	return _redoStack.size();
}

void CommandManager::TrimUndoHistory()
{
	if (_historyLimit == 0)
	{
		_historyLimit = 1;
	}

	if (_undoStack.size() <= _historyLimit)
	{
		return;
	}

	const size_t removeCount = _undoStack.size() - _historyLimit;
	_undoStack.erase(_undoStack.begin(), _undoStack.begin() + (std::ptrdiff_t)removeCount);
}
