#pragma once

#include "Brush.h"
#include "DependencyObject.h"

/**
 * WPF TextElement property owner.
 *
 * CUI does not expose the document-object model yet. This shell exists so
 * text properties can nevertheless use the same stable dependency-property
 * identities and owner boundaries as WPF instead of being registered on the
 * native Control behavior host.
 */
class TextElement : public DependencyObject
{
public:
	static const DependencyProperty& ForegroundProperty();
	static const DependencyProperty& BackgroundProperty();
	static void RegisterDependencyProperties();

	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
};
