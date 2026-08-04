#pragma once

#include "Panel.h"

/**
 * WPF Canvas behavior host.
 *
 * Panel remains the non-authored native base shared by all panel layouts;
 * Canvas is the concrete XAML type that owns the absolute-layout identity.
 */
class Canvas : public Panel
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Pane, L"Canvas");
	}

public:
	Canvas() = default;
	UIClass Type() override { return UIClass::UI_Canvas; }
	static float GetLeft(Control& element) noexcept
	{
		return element.GetCanvasLeft();
	}
	static void SetLeft(Control& element, float value)
	{
		element.SetCanvasLeft(value);
	}
	static float GetTop(Control& element) noexcept
	{
		return element.GetCanvasTop();
	}
	static void SetTop(Control& element, float value)
	{
		element.SetCanvasTop(value);
	}
	static float GetRight(Control& element) noexcept
	{
		return element.GetCanvasRight();
	}
	static void SetRight(Control& element, float value)
	{
		element.SetCanvasRight(value);
	}
	static float GetBottom(Control& element) noexcept
	{
		return element.GetCanvasBottom();
	}
	static void SetBottom(Control& element, float value)
	{
		element.SetCanvasBottom(value);
	}
	static void RegisterDependencyProperties()
	{
		Panel::RegisterDependencyProperties();
	}
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
};
