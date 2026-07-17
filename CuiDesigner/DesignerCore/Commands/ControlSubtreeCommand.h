#pragma once

#include "ControlPlacementCommand.h"
#include "../../DesignerModel/DesignDocument.h"
#include <memory>
#include <string>
#include <vector>

class DesignerCanvas;

struct DesignerSubtreeIdentity
{
	std::wstring Name;
	UIClass Type = UIClass::UI_Base;

	bool operator==(const DesignerSubtreeIdentity&) const = default;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

struct DesignerSubtreeRootAttachmentState
{
	bool HasToolBarSizeOverride = false;
	SIZE ToolBarSizeOverride{ -1, -1 };

	bool operator==(
		const DesignerSubtreeRootAttachmentState& other) const noexcept
	{
		return HasToolBarSizeOverride == other.HasToolBarSizeOverride
			&& ToolBarSizeOverride.cx == other.ToolBarSizeOverride.cx
			&& ToolBarSizeOverride.cy == other.ToolBarSizeOverride.cy;
	}
};

/** Persisted, normalized identity and state for one or more top-level subtrees. */
struct DesignerControlSubtreeSnapshot
{
	std::vector<DesignerSubtreeIdentity> Identities;
	DesignerControlPlacementSnapshot RootPlacements;
	std::vector<DesignerSubtreeRootAttachmentState> RootAttachments;
	std::vector<DesignerModel::DesignNode> Nodes;

	bool EquivalentTo(
		const DesignerControlSubtreeSnapshot& other) const noexcept;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

/**
 * Reversible Add/Delete delta.
 *
 * Attached controls are owned exclusively by the runtime tree. While absent,
 * this command owns every detached root through unique_ptr. It never retains
 * ownership of an attached control and never stores a full DesignDocument.
 */
class ControlSubtreeCommand final : public IDesignerCommand
{
public:
	ControlSubtreeCommand(
		DesignerCanvas* canvas,
		DesignerControlSubtreeSnapshot snapshot,
		std::vector<std::wstring> beforeSelectionNames,
		std::vector<std::wstring> afterSelectionNames,
		std::wstring beforePrimarySelectionName,
		std::wstring afterPrimarySelectionName,
		bool presentAfter,
		std::wstring label,
		bool skipInitialExecute);
	~ControlSubtreeCommand() override;

	static bool Capture(
		DesignerCanvas* canvas,
		const std::vector<std::shared_ptr<DesignerControl>>& roots,
		DesignerControlSubtreeSnapshot& out,
		std::wstring* outError = nullptr);

	DesignerDocumentTransactionResult Execute() override;
	DesignerDocumentTransactionResult Undo() override;
	std::wstring GetLabel() const override;
	bool TryMergeWith(IDesignerCommand&) noexcept override { return false; }
	size_t GetEstimatedMemoryUsage() const noexcept override;

private:
	struct DetachedPayload;

	DesignerDocumentTransactionResult Apply(
		bool expectedPresent,
		bool desiredPresent,
		const std::vector<std::wstring>& selectionNames,
		const std::wstring& primarySelectionName);
	bool CaptureCurrent(
		DesignerControlSubtreeSnapshot& out,
		std::wstring* outError) const;
	static std::shared_ptr<DesignerControl> FindControl(
		DesignerCanvas* canvas,
		const std::wstring& name,
		UIClass type,
		bool requireType = true);
	bool ResolveParent(
		const DesignerControlPlacementState& state,
		Control*& runtimeParent,
		Control*& designerParent,
		std::wstring* outError) const;
	bool ValidatePresent(std::wstring* outError) const;
	bool ValidateAbsent(std::wstring* outError) const;
	bool DetachToStorage(std::wstring* outError);
	bool AttachFromStorage(std::wstring* outError);

	DesignerCanvas* _canvas = nullptr;
	DesignerControlSubtreeSnapshot _snapshot;
	std::vector<std::wstring> _beforeSelectionNames;
	std::vector<std::wstring> _afterSelectionNames;
	std::wstring _beforePrimarySelectionName;
	std::wstring _afterPrimarySelectionName;
	bool _presentAfter = false;
	std::wstring _label;
	bool _skipInitialExecute = false;
	std::unique_ptr<DetachedPayload> _detached;
	size_t _estimatedMemoryUsage = 0;
};
