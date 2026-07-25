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

	bool RegisterCore(DesignerCustomEditorDescriptor descriptor)
	{
		if (descriptor.Id.empty() || descriptor.ButtonText.empty()) return false;
		std::lock_guard<std::mutex> lock(RegistryMutex());
		auto& registry = Registry();
		const auto existing = std::find_if(registry.begin(), registry.end(),
			[&](const DesignerCustomEditorDescriptor& current)
			{
				return current.TargetType == descriptor.TargetType
					&& current.Id == descriptor.Id;
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
			RegisterCore({ L"Definitions", UIClass::UI_Grid,
				L"编辑行/列...", 10, DesignerCustomEditorKind::GridDefinitions });
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
		return left.Id < right.Id;
	});
	return result;
}
}
