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
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
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
	bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;

private:
	friend class ItemsControl;
	friend struct cui::framework::TemplateAccess;

	Panel* GetItemsHost() const noexcept { return _itemsHost; }
	Panel* SetItemsHost(std::unique_ptr<Panel> value);
	std::unique_ptr<Panel> DetachItemsHost();

	Panel* _itemsHost = nullptr;
	bool _changingItemsHost = false;
	bool _contentLayoutPending = true;
};
