#include "ListBox.h"

ListBox::ListBox(int x, int y, int width, int height)
	: Selector(x, y, width, height)
{
	BackColor = cui::theme::palette::Surface;
	BorderThickness = 1.0f;
}

void ListBox::EnsureBindingPropertiesRegistered()
{
	Selector::EnsureBindingPropertiesRegistered();
}
