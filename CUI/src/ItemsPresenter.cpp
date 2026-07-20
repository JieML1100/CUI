#include "ItemsPresenter.h"

#include <stdexcept>
#include <utility>

ItemsPresenter::ItemsPresenter(int x, int y, int width, int height)
	: GridPanel(x, y, width, height)
{
	BorderThickness = 0.0f;
	BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	SetAutoSize(true, true);
	HAlign = HorizontalAlignment::Stretch;
	VAlign = VerticalAlignment::Top;
}

void ItemsPresenter::EnsureBindingPropertiesRegistered()
{
	GridPanel::EnsureBindingPropertiesRegistered();
}

bool ItemsPresenter::ValidateChildCollection(
	std::span<Control* const> children,
	std::string& error) const
{
	if (_changingItemsHost) return true;
	if (!_itemsHost && children.empty()) return true;
	if (_itemsHost && children.size() == 1 && children.front() == _itemsHost)
		return true;
	error = "ItemsPresenter children are owned by its templated ItemsControl";
	return false;
}

void ItemsPresenter::ConfigureItemsHost(Panel& host)
{
	host.GridRow = 0;
	host.GridColumn = 0;
	host.GridRowSpan = 1;
	host.GridColumnSpan = 1;
	host.HAlign = HorizontalAlignment::Stretch;
	host.VAlign = VerticalAlignment::Top;
}

Panel* ItemsPresenter::SetItemsHost(std::unique_ptr<Panel> value)
{
	if (!value) throw std::invalid_argument("ItemsPresenter ItemsHost is null");
	if (_itemsHost)
		throw std::logic_error("ItemsPresenter already owns an ItemsHost");
	ConfigureItemsHost(*value);
	_itemsHost = value.get();
	_changingItemsHost = true;
	try
	{
		AddOwned(std::move(value));
		_changingItemsHost = false;
	}
	catch (...)
	{
		_changingItemsHost = false;
		_itemsHost = nullptr;
		throw;
	}
	InvalidateLayout();
	return _itemsHost;
}

std::unique_ptr<Panel> ItemsPresenter::DetachItemsHost()
{
	if (!_itemsHost) return {};
	auto* previous = _itemsHost;
	_itemsHost = nullptr;
	_changingItemsHost = true;
	std::unique_ptr<Control> detached;
	try
	{
		detached = DetachControl(previous);
		_changingItemsHost = false;
	}
	catch (...)
	{
		_changingItemsHost = false;
		_itemsHost = previous;
		throw;
	}
	return std::unique_ptr<Panel>(static_cast<Panel*>(detached.release()));
}
