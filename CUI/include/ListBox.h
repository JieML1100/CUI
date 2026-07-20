#pragma once

#include "Selector.h"

/** Templated single-selection list. */
class ListBox final : public Selector
{
public:
	ListBox(int x = 0, int y = 0, int width = 200, int height = 160);
	UIClass Type() override { return UIClass::UI_ListBox; }
	void EnsureBindingPropertiesRegistered() override;
};
