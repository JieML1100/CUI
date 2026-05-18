#pragma once

#include "../DesignerTypes.h"
#include <functional>
#include <memory>
#include <vector>

class Control;

class HitTestService
{
public:
	static bool IsDescendantOf(Control* ancestor, Control* node);
	static bool IsContainerControl(Control* control);
	static std::shared_ptr<DesignerControl> HitTestControl(
		Control* root,
		const std::vector<std::shared_ptr<DesignerControl>>& designerControls,
		POINT pt,
		bool preferParentContainer);
	static Control* FindBestContainerAtPoint(
		const std::vector<std::shared_ptr<DesignerControl>>& designerControls,
		POINT ptCanvas,
		Control* ignore,
		const std::function<RECT(Control*)>& getControlRectInCanvas);
};