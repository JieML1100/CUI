#pragma once

#include "Selector.h"

/** Templated list selector. ListView specializes its item container. */
class ListBox : public Selector
{
public:
	ListBox();
	UIClass Type() override { return UIClass::UI_ListBox; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
	// ListBox owns an item viewport even when a theme does not supply the
	// conventional ScrollViewer template. Generated containers must not paint
	// beyond the selector's arranged slot.
	bool ClipsChildren() override { return true; }
};
