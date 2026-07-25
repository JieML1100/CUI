#pragma once

#include "DesignerCustomEditorCatalog.h"
#include "DesignerTypes.h"

#include <string>
#include <vector>

struct DesignerGridTrackSnapshot
{
	float Value = 0.0f;
	int Unit = 0;
	float Minimum = 0.0f;
	float Maximum = 0.0f;

	bool operator==(const DesignerGridTrackSnapshot&) const = default;
};

/**
 * Small, typed state for one modal structure editor. Unlike a document
 * snapshot it carries no unrelated controls, resources, styles, or bindings.
 */
struct DesignerStructureSnapshot
{
	int StableId = 0;
	std::wstring TargetName;
	UIClass TargetType = UIClass::UI_Base;
	DesignerCustomEditorKind Kind =
		DesignerCustomEditorKind::GridDefinitions;
	std::vector<DesignerGridTrackSnapshot> GridRows;
	std::vector<DesignerGridTrackSnapshot> GridColumns;

	bool operator==(const DesignerStructureSnapshot&) const = default;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

namespace DesignerStructureEdit
{
	/** Structure kinds that do not transfer Designer-owned child controls. */
	bool SupportsDelta(DesignerCustomEditorKind kind) noexcept;

	bool Capture(
		DesignerControl& control,
		DesignerCustomEditorKind kind,
		DesignerStructureSnapshot& output,
		std::wstring* outError = nullptr);

	/** Restores one previously captured state without replacing the control. */
	bool Restore(
		DesignerControl& control,
		const DesignerStructureSnapshot& snapshot,
		std::wstring* outError = nullptr);
}
