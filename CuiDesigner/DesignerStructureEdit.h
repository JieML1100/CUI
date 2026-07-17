#pragma once

#include "DesignerCustomEditorCatalog.h"
#include "DesignerTypes.h"

#include <string>
#include <vector>

struct DesignerGridViewColumnSnapshot
{
	std::wstring Name;
	float Width = 120.0f;
	int Type = 0;
	bool CanEdit = true;
	std::vector<std::wstring> ComboBoxItems;
	std::wstring ButtonText;

	bool operator==(const DesignerGridViewColumnSnapshot&) const = default;
};

struct DesignerTreeNodeSnapshot
{
	std::wstring Text;
	bool Expand = false;
	std::vector<DesignerTreeNodeSnapshot> Children;

	bool operator==(const DesignerTreeNodeSnapshot&) const = default;
};

struct DesignerMenuItemSnapshot
{
	std::wstring Text;
	int Id = 0;
	std::wstring Shortcut;
	bool Separator = false;
	bool Enable = true;
	std::vector<DesignerMenuItemSnapshot> Children;

	bool operator==(const DesignerMenuItemSnapshot&) const = default;
};

struct DesignerGridTrackSnapshot
{
	float Value = 0.0f;
	int Unit = 0;
	float Minimum = 0.0f;
	float Maximum = 0.0f;

	bool operator==(const DesignerGridTrackSnapshot&) const = default;
};

struct DesignerStatusBarPartSnapshot
{
	std::wstring Text;
	int Width = 0;

	bool operator==(const DesignerStatusBarPartSnapshot&) const = default;
};

/**
 * ComboBox Items and SelectedIndex are one logical edit. Replacing Items may
 * coerce the stored Binding value before the editor installs a Local value, so
 * both value sources and the Designer persistence state must travel together.
 */
struct DesignerComboBoxSnapshot
{
	std::vector<std::wstring> Items;
	int EffectiveSelectedIndex = 0;
	bool HasLocalSelectedIndex = false;
	int LocalSelectedIndex = 0;
	bool HasBindingSelectedIndex = false;
	int BindingSelectedIndex = 0;
	bool HasConfiguredBinding = false;
	DesignerDataBinding ConfiguredBinding;
	bool HasTrackedSelectedIndex = false;
	DesignerStyleValue TrackedSelectedIndex;

	bool operator==(const DesignerComboBoxSnapshot&) const = default;
	size_t GetEstimatedMemoryUsage() const noexcept;
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
		DesignerCustomEditorKind::GridViewColumns;
	DesignerComboBoxSnapshot ComboBox;
	std::vector<DesignerGridViewColumnSnapshot> GridViewColumns;
	std::vector<DesignerTreeNodeSnapshot> TreeNodes;
	std::vector<DesignerMenuItemSnapshot> MenuItems;
	std::vector<DesignerGridTrackSnapshot> GridRows;
	std::vector<DesignerGridTrackSnapshot> GridColumns;
	std::vector<DesignerStatusBarPartSnapshot> StatusBarParts;

	bool operator==(const DesignerStructureSnapshot&) const = default;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

namespace DesignerStructureEdit
{
	/** Structure kinds that do not transfer Designer-owned child controls. */
	bool SupportsDelta(DesignerCustomEditorKind kind) noexcept;

	bool Capture(
		const DesignerControl& control,
		DesignerCustomEditorKind kind,
		DesignerStructureSnapshot& output,
		std::wstring* outError = nullptr);

	/** Restores one previously captured state without replacing the control. */
	bool Restore(
		DesignerControl& control,
		const DesignerStructureSnapshot& snapshot,
		std::wstring* outError = nullptr);
}
