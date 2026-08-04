#pragma once

#include "Control.h"

/**
 * WPF-style single-child structural element.
 *
 * Decorator is abstract in XAML but is a usable native behavior host for
 * XAML-defined component types. It owns only the Child relationship and
 * FrameworkElement layout semantics; it does not expose Control chrome or a
 * ControlTemplate.
 */
class Decorator : public Control
{
public:
	Decorator() = default;
	~Decorator() override = default;

	UIClass Type() override { return UIClass::UI_Decorator; }
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	Control* GetChild() const noexcept;
	Control* SetChild(std::unique_ptr<Control> value);
	bool TrySetChild(std::unique_ptr<Control>& value) noexcept;
	std::unique_ptr<Control> DetachChild();

	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void Arrange(cui::core::Rect finalRect) override;
	cui::core::Point GetVisualChildrenLayoutOriginDip() override;

protected:
	virtual cui::core::Insets GetDecoratorInsets() const noexcept
	{
		return {};
	}
	void RequestLayout() override;
	void OnComputedLayoutSizeChanged() override;
	void PerformPendingLayout() override;
	bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	void OnVisualChildCollectionChanged(
		const CollectionChangedEventArgs& change,
		std::span<Control* const> previousChildren) override;

private:
	bool _childLayoutPending = true;
};
