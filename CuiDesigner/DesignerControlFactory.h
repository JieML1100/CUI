#pragma once

#include "DesignerTypes.h"
#include <memory>

namespace DesignerControlFactory
{
	/** Creates a lightweight representative of a Designer-supported UIClass. */
	std::unique_ptr<Control> Create(UIClass type, int x = 0, int y = 0);
}
