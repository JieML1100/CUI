#pragma once

#include "../DesignerTypes.h"
#include <functional>

class Control;

class LayoutBridge
{
public:
	static Control* NormalizeContainerForDrop(Control* container);
	static bool CanAcceptChild(Control* container, UIClass childType);
	static void AttachChild(Control* container, Control* child);
	static void ApplyNewChildLayout(Control* container, Control* child, POINT local, POINT dropLocal);
	static void ApplyExistingChildLayout(
		Control* container,
		Control* child,
		POINT local,
		POINT dropLocalCenter,
		bool containerChanged,
		const RECT& originalRectInCanvas,
		const std::function<void(const RECT&)>& applyRectToControl);
	static void RefreshContainerLayout(Control* container);
};