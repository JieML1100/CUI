#pragma once

#include "DesignerDocumentTransaction.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class IDesignerCommand
{
public:
	virtual ~IDesignerCommand() = default;
	virtual DesignerDocumentTransactionResult Execute() = 0;
	virtual DesignerDocumentTransactionResult Undo() = 0;
	virtual std::wstring GetLabel() const = 0;
	virtual bool TryMergeWith(IDesignerCommand&) noexcept { return false; }
	virtual size_t GetEstimatedMemoryUsage() const noexcept
	{
		return sizeof(IDesignerCommand);
	}
};

class CommandManager
{
public:
	DesignerDocumentTransactionResult Execute(
		std::unique_ptr<IDesignerCommand> command);
	bool CanUndo() const;
	bool CanRedo() const;
	DesignerDocumentTransactionResult Undo();
	DesignerDocumentTransactionResult Redo();
	std::wstring GetUndoLabel() const;
	std::wstring GetRedoLabel() const;
	void Clear();
	void ResetAsUnsaved();
	void MarkSaved();
	bool IsDirty() const noexcept;
	uint64_t GetCurrentStateId() const noexcept;
	uint64_t GetSavedStateId() const noexcept;
	void SetHistoryLimit(size_t historyLimit);
	void SetHistoryMemoryLimit(size_t byteLimit);
	size_t GetUndoCount() const noexcept;
	size_t GetRedoCount() const noexcept;
	size_t GetHistoryMemoryLimit() const noexcept;
	size_t GetHistoryMemoryUsage() const noexcept;

private:
	struct HistoryEntry
	{
		std::unique_ptr<IDesignerCommand> Command;
		uint64_t BeforeStateId = 0;
		uint64_t AfterStateId = 0;
		size_t EstimatedBytes = 0;
	};

	void TrimHistory();
	void RemoveUndoEntry(size_t index);
	void RemoveRedoEntry(size_t index);
	void ClearRedoHistory();
	uint64_t AllocateStateId() noexcept;

	std::vector<HistoryEntry> _undoStack;
	std::vector<HistoryEntry> _redoStack;
	size_t _historyLimit = 128;
	size_t _historyMemoryLimit = 64ull * 1024ull * 1024ull;
	size_t _historyMemoryUsage = 0;
	uint64_t _currentStateId = 0;
	uint64_t _savedStateId = 0;
	uint64_t _nextStateId = 1;
};
