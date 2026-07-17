#pragma once

#include "CommandManager.h"
#include "DesignerDocumentTransaction.h"
#include "../DesignerModel/DesignDocument.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class DesignerCanvas;

class DesignerCommandCoordinator
{
public:
	using DocumentEditOperation =
		std::function<bool(std::wstring& error)>;

	explicit DesignerCommandCoordinator(DesignerCanvas* canvas);

	DesignerDocumentTransactionResult Execute(
		std::unique_ptr<IDesignerCommand> command);
	DesignerDocumentTransactionResult Undo();
	DesignerDocumentTransactionResult Redo();
	std::wstring GetUndoLabel() const;
	std::wstring GetRedoLabel() const;
	bool IsDocumentDirty() const noexcept;
	uint64_t GetCurrentDocumentStateId() const noexcept;
	uint64_t GetSavedDocumentStateId() const noexcept;
	void SetHistoryMemoryLimit(size_t byteLimit);
	size_t GetHistoryMemoryLimit() const noexcept;
	size_t GetHistoryMemoryUsage() const noexcept;
	size_t GetUndoCount() const noexcept;
	size_t GetRedoCount() const noexcept;
	bool HasActiveDocumentTransaction() const noexcept;
	DesignerDocumentTransactionResult MarkDocumentSaved();
	DesignerDocumentTransactionResult ResetHistoryAsSaved();
	DesignerDocumentTransactionResult ResetHistoryAsUnsaved();

	DesignerDocumentTransactionResult ExecuteDocumentTransaction(
		const std::wstring& label,
		const DocumentEditOperation& applyChange);
	DesignerDocumentTransactionResult BeginDocumentTransaction(
		const std::wstring& label);
	DesignerDocumentTransactionResult CommitDocumentTransaction();
	DesignerDocumentTransactionResult RollbackDocumentTransaction();
	DesignerDocumentTransactionResult CancelDocumentTransaction();

private:
	struct InteractionSnapshot
	{
		std::unique_ptr<DesignerModel::DesignDocument> Document;
		std::vector<std::wstring> SelectionNames;
		std::wstring PrimarySelectionName;
		std::wstring Label;
	};

	DesignerDocumentTransactionResult CaptureInteractionSnapshot(
		const std::wstring& label);
	DesignerDocumentTransactionResult CommitCapturedSnapshot();
	DesignerDocumentTransactionResult RollbackCapturedSnapshot();
	DesignerDocumentTransactionResult CancelCapturedSnapshot();
	void ClearInteractionSnapshot();
	bool RestoreDocumentAndSelection(
		const DesignerModel::DesignDocument& document,
		const std::vector<std::wstring>& selectionNames,
		const std::wstring& primarySelectionName,
		std::wstring& error) const;

	DesignerCanvas* _canvas = nullptr;
	CommandManager _commandManager;
	InteractionSnapshot _interactionSnapshot;
};
