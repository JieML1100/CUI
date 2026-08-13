#pragma once

#include "Panel.h"

class ItemsControl;
namespace cui::framework
{
	struct TemplateAccess;
}

/**
 * WPF-style ControlTemplate slot for an ItemsControl's generated ItemsHost.
 *
 * The presenter owns no authored children. ItemsControl moves its current
 * ItemsPanelTemplate host into this slot while a template is active, so item
 * generation, selection, grouping, and virtualization keep one visual tree.
 */
class ItemsPresenter final : public Control
{
public:
	ItemsPresenter();
	UIClass Type() override { return UIClass::UI_ItemsPresenter; }
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
protected:
	void OnRender() override;
public:
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void Arrange(cui::core::Rect finalRect) override;

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Pane, L"ItemsPresenter");
	}
	void RequestLayout() override;
	void OnComputedLayoutSizeChanged() override;
	void PerformPendingLayout() override;
	void OnLocalMeasurePathInvalidated() override
	{
		_contentLayoutPending = true;
	}
	bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;

private:
	friend class ItemsControl;
	friend struct cui::framework::TemplateAccess;

	Panel* GetItemsHost() const noexcept { return _itemsHost; }
	Panel* SetItemsHost(std::unique_ptr<Panel> value);
	std::unique_ptr<Panel> DetachItemsHost();

	struct ItemsHostMutationFrame;

	Panel* _itemsHost = nullptr;
	// Synchronous parent-change callbacks can enter another visual mutation.
	// Keep the exact active transaction on the call stack instead of exposing
	// every collection shape through one shared boolean.
	ItemsHostMutationFrame* _activeItemsHostMutation = nullptr;
	bool _contentLayoutPending = true;
};
