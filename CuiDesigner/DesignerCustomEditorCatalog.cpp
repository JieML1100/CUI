#include "DesignerCustomEditorCatalog.h"
#include <algorithm>
#include <cwctype>
#include <mutex>

namespace DesignerCustomEditorCatalog
{
namespace
{
	std::mutex& RegistryMutex()
	{
		static std::mutex value;
		return value;
	}

	std::vector<DesignerCustomEditorDescriptor>& Registry()
	{
		static std::vector<DesignerCustomEditorDescriptor> value;
		return value;
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	bool RegisterCore(DesignerCustomEditorDescriptor descriptor)
	{
		if (descriptor.Id.empty() || descriptor.ButtonText.empty()) return false;
		std::lock_guard<std::mutex> lock(RegistryMutex());
		auto& registry = Registry();
		const auto id = Lower(descriptor.Id);
		const auto existing = std::find_if(registry.begin(), registry.end(),
			[&](const DesignerCustomEditorDescriptor& current)
			{
				return current.TargetType == descriptor.TargetType
					&& Lower(current.Id) == id;
			});
		if (existing == registry.end()) registry.push_back(std::move(descriptor));
		else *existing = std::move(descriptor);
		return true;
	}

	void EnsureDefaultsRegistered()
	{
		static std::once_flag once;
		std::call_once(once, []
		{
			RegisterCore({ L"Items", UIClass::UI_ComboBox,
				L"编辑下拉项...", 10, DesignerCustomEditorKind::ComboBoxItems });
			RegisterCore({ L"Columns", UIClass::UI_GridView,
				L"编辑列...", 10, DesignerCustomEditorKind::GridViewColumns });
			RegisterCore({ L"Pages", UIClass::UI_TabControl,
				L"编辑页...", 10, DesignerCustomEditorKind::TabControlPages });
			RegisterCore({ L"Buttons", UIClass::UI_ToolBar,
				L"编辑文字按钮...", 10, DesignerCustomEditorKind::ToolBarButtons });
			RegisterCore({ L"Nodes", UIClass::UI_TreeView,
				L"编辑节点...", 10, DesignerCustomEditorKind::TreeViewNodes });
			RegisterCore({ L"Definitions", UIClass::UI_GridPanel,
				L"编辑行/列...", 10, DesignerCustomEditorKind::GridPanelDefinitions });
			RegisterCore({ L"Items", UIClass::UI_Menu,
				L"编辑菜单项...", 10, DesignerCustomEditorKind::MenuItems });
			RegisterCore({ L"Parts", UIClass::UI_StatusBar,
				L"编辑分段...", 10, DesignerCustomEditorKind::StatusBarParts });
		});
	}
}

bool Register(DesignerCustomEditorDescriptor descriptor)
{
	EnsureDefaultsRegistered();
	return RegisterCore(std::move(descriptor));
}

std::vector<DesignerCustomEditorDescriptor> GetEditors(UIClass targetType)
{
	EnsureDefaultsRegistered();
	std::vector<DesignerCustomEditorDescriptor> result;
	{
		std::lock_guard<std::mutex> lock(RegistryMutex());
		for (const auto& descriptor : Registry())
			if (descriptor.TargetType == targetType) result.push_back(descriptor);
	}
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
	{
		if (left.Order != right.Order) return left.Order < right.Order;
		return Lower(left.Id) < Lower(right.Id);
	});
	return result;
}
}
