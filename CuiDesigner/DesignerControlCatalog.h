#pragma once

#include "DesignerTypes.h"

#include <optional>
#include <vector>

/** The product toolbox is a read-only projection of the XAML runtime Schema. */
namespace DesignerControlCatalog
{
	std::vector<DesignerControlDescriptor> BuiltInDescriptors();
	std::optional<DesignerControlDescriptor> FindBuiltIn(UIClass nativeType);
}
