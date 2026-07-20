#pragma once

#include "Layout/GridPanel.h"

/**
 * WPF-style ControlTemplate slot for an ItemsControl's generated ItemsHost.
 *
 * The presenter owns no authored children. ItemsControl moves its current
 * ItemsPanelTemplate host into this slot while a template is active, so item
 * generation, selection, grouping, and virtualization keep one visual tree.
 */
class ItemsPresenter final : public GridPanel
{
public:
	ItemsPresenter(int x = 0, int y = 0, int width = 0, int height = 0);
	UIClass Type() override { return UIClass::UI_ItemsPresenter; }
	void EnsureBindingPropertiesRegistered() override;

	Panel* GetItemsHost() const noexcept { return _itemsHost; }
	Panel* SetItemsHost(std::unique_ptr<Panel> value);
	std::unique_ptr<Panel> DetachItemsHost();

protected:
	bool ValidateChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;

private:
	Panel* _itemsHost = nullptr;
	bool _changingItemsHost = false;
	void ConfigureItemsHost(Panel& host);
};
