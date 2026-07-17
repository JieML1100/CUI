#pragma once

#include "../CommandManager.h"
#include "EventHandlerCodeMigration.h"
#include "../../DesignerTypes.h"

#include <string>
#include <vector>

class DesignerCanvas;

struct DesignerEventHandlerValueSnapshot
{
	bool Exists = false;
	std::wstring StoredHandler;

	bool EquivalentTo(
		const DesignerEventHandlerValueSnapshot& other) const noexcept
	{
		return Exists == other.Exists
			&& (!Exists || StoredHandler == other.StoredHandler);
	}

	size_t GetEstimatedMemoryUsage() const noexcept;
};

/** One Form/control event mapping delta addressed by stable design identity. */
struct DesignerEventHandlerDelta
{
	bool IsForm = false;
	int StableId = 0;
	UIClass ControlType = UIClass::UI_Base;
	std::wstring SubjectName;
	std::wstring EventName;
	DesignerEventHandlerValueSnapshot Before;
	DesignerEventHandlerValueSnapshot After;

	size_t GetEstimatedMemoryUsage() const noexcept;
};

/**
 * Reversible event-map delta. It validates every expected value before changing
 * any target, so stale history never overwrites newer event edits.
 */
class EventHandlerCommand final : public IDesignerCommand
{
public:
	EventHandlerCommand(
		DesignerCanvas* canvas,
		std::vector<DesignerEventHandlerDelta> deltas,
		std::vector<std::wstring> selectionNames,
		std::wstring primarySelectionName,
		std::wstring label,
		DesignerEventHandlerCodeMigration codeMigration = {});

	DesignerDocumentTransactionResult Execute() override;
	DesignerDocumentTransactionResult Undo() override;
	std::wstring GetLabel() const override;
	size_t GetEstimatedMemoryUsage() const noexcept override;

private:
	DesignerDocumentTransactionResult Apply(bool undo) const;

	DesignerCanvas* _canvas = nullptr;
	std::vector<DesignerEventHandlerDelta> _deltas;
	std::vector<std::wstring> _selectionNames;
	std::wstring _primarySelectionName;
	std::wstring _label;
	DesignerEventHandlerCodeMigration _codeMigration;
	size_t _estimatedMemoryUsage = 0;
};
