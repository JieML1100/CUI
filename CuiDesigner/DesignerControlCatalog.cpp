#include "DesignerControlCatalog.h"
#include "../CuiRuntime/include/XamlRuntimeSchema.h"

namespace DesignerControlCatalog
{
std::vector<DesignerControlDescriptor> BuiltInDescriptors()
{
	std::vector<DesignerControlDescriptor> result;
	for (const auto& type : CuiRuntime::XamlRuntimeSchema::EnumerateBuiltInTypes())
	{
		if (!type.IsDesignerToolboxType()) continue;
		result.push_back({
			type.NativeType,
			type.TypeId.LocalName,
			std::wstring(type.DesignerDisplayName),
			{ type.DesignerDefaultWidth, type.DesignerDefaultHeight },
			type.DesignerIsContainer,
			std::wstring(type.DesignerCategory) });
	}
	return result;
}

std::optional<DesignerControlDescriptor> FindBuiltIn(UIClass nativeType)
{
	const auto* type = CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(nativeType);
	if (!type || !type->IsDesignerToolboxType()) return std::nullopt;
	return DesignerControlDescriptor{
		type->NativeType,
		type->TypeId.LocalName,
		std::wstring(type->DesignerDisplayName),
		{ type->DesignerDefaultWidth, type->DesignerDefaultHeight },
		type->DesignerIsContainer,
		std::wstring(type->DesignerCategory) };
}
}
