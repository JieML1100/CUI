#include "DesignerControlFactory.h"

#include "../CuiRuntime/include/XamlRuntimeSchema.h"
#include "../CUI/include/Canvas.h"
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
		: CuiRuntime::XamlRuntimeSchema::CreateNativeControl(type);
	if (control)
	{
		// Designer-created XAML nodes start from metadata/theme defaults, not
		// native constructor locals. Placement below is authored state.
		(void)control->ClearPropertyValues();
		Canvas::SetLeft(*(control), static_cast<float>(x));
		Canvas::SetTop(*(control), static_cast<float>(y));
		Canvas::SetRight(*(control), cui::layout::UnsetCanvasOffset);
		Canvas::SetBottom(*(control), cui::layout::UnsetCanvasOffset);
	}
	return control;
}
}
