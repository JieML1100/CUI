#pragma once

#include <memory>
#include <string>
#include <vector>

class IDesignerCommand
{
public:
	virtual ~IDesignerCommand() = default;
	virtual bool Execute() = 0;
	virtual void Undo() = 0;
	virtual std::wstring GetLabel() const = 0;
};

class CommandManager
{
public:
	bool Execute(std::unique_ptr<IDesignerCommand> command);
	bool CanUndo() const;
	bool CanRedo() const;
	bool Undo();
	bool Redo();
	void Clear();
	void SetHistoryLimit(size_t historyLimit);
	size_t GetUndoCount() const;
	size_t GetRedoCount() const;

private:
	void TrimUndoHistory();

	std::vector<std::unique_ptr<IDesignerCommand>> _undoStack;
	std::vector<std::unique_ptr<IDesignerCommand>> _redoStack;
	size_t _historyLimit = 128;
};