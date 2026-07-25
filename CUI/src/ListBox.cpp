#include "ListBox.h"

ListBox::ListBox()
	: Selector()
{
	RendererBackgroundColor = cui::theme::palette::Surface;
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(1.0f)),
		DependencyPropertyValueSource::Theme);
}

void ListBox::RegisterDependencyProperties()
{
	Selector::RegisterDependencyProperties();
}
