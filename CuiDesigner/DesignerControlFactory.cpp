#include "DesignerControlFactory.h"

#include "DesignerModel/DesignDocumentMaterializer.h"
#include "FakeWebBrowser.h"

namespace DesignerControlFactory
{
std::unique_ptr<Control> Create(UIClass type, int x, int y)
{
	// Native/browser-backed controls need a design-safe visual. Every ordinary
	// control comes from the production registry so the Designer cannot drift
	// behind the XAML runtime's public type surface.
	std::unique_ptr<Control> control = type == UIClass::UI_WebBrowser
		? std::make_unique<FakeWebBrowser>(0, 0, 500, 360)
		: DesignerModel::DesignDocumentMaterializer::CreateRuntimeControl(type);
	if (control) control->Location = { x, y };
	return control;
}
}
