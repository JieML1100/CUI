#pragma once

#include "CommandManager.h"
#include "../DesignerModel/DesignDocument.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class DesignerCanvas;

class DesignerCommandCoordinator
{
public:
	explicit DesignerCommandCoordinator(DesignerCanvas* canvas);

	bool Execute(std::unique_ptr<IDesignerCommand> command);
	bool Undo();
	bool Redo();

	void ExecuteDocumentSnapshotCommand(const std::wstring& label, const std::function<void()>& applyChange);
	void BeginInteractionSnapshot(const std::wstring& label);
	void CommitInteractionSnapshot();
	void ClearInteractionSnapshot();

private:
	struct InteractionSnapshot
	{
		std::unique_ptr<DesignerModel::DesignDocument> Document;
		std::vector<std::wstring> SelectionNames;
		std::wstring PrimarySelectionName;
		std::wstring Label;
	};

	DesignerCanvas* _canvas = nullptr;
	CommandManager _commandManager;
	InteractionSnapshot _interactionSnapshot;
};