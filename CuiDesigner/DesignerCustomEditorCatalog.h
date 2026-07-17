#pragma once

#include "../CUI/include/Control.h"
#include <string>
#include <vector>

enum class DesignerCustomEditorKind : unsigned char
{
	ComboBoxItems,
	GridViewColumns,
	TabControlPages,
	ToolBarButtons,
	TreeViewNodes,
	GridPanelDefinitions,
	MenuItems,
	StatusBarParts
};

struct DesignerCustomEditorDescriptor
{
	std::wstring Id;
	UIClass TargetType = UIClass::UI_Base;
	std::wstring ButtonText;
	int Order = 0;
	DesignerCustomEditorKind Kind = DesignerCustomEditorKind::ComboBoxItems;
};

/** Designer-only registry for structural/collection editors. */
namespace DesignerCustomEditorCatalog
{
	/** Registers or replaces one editor with the same target type and id. */
	bool Register(DesignerCustomEditorDescriptor descriptor);

	/** Returns applicable editors in stable Order/Id order. */
	std::vector<DesignerCustomEditorDescriptor> GetEditors(UIClass targetType);
}
