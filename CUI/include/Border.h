#pragma once

#include "Decorator.h"

/**
 * WPF-style Border decorator.
 *
 * Border is the only structural primitive that owns Padding and border chrome.
 * Panels remain layout-only elements and therefore cannot acquire these
 * members merely because the native implementation shares a Control host.
 */
class Border final : public Decorator
{
public:
	Border() = default;
	UIClass Type() override { return UIClass::UI_Border; }
	/** WPF identities: Border owns its brush and thickness chrome. */
	static const DependencyProperty& BorderBrushProperty();
	static const DependencyProperty& BackgroundProperty();
	static const DependencyProperty& BorderThicknessProperty();
	static const DependencyProperty& PaddingProperty();
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

protected:
	cui::core::Insets GetDecoratorInsets() const noexcept override;
	void OnRender() override;
};
