#include "DesignerControlCatalog.h"

namespace DesignerControlCatalog
{
std::vector<DesignerControlDescriptor> BuiltInDescriptors()
{
	std::vector<DesignerControlDescriptor> result;
	const auto controls = ControlRegistry::GetAvailableControls();
	result.reserve(controls.size());
	for (const auto& metadata : controls)
		result.push_back(DesignerControlDescriptor::BuiltIn(metadata));
	return result;
}
}
