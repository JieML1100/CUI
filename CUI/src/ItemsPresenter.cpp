#include "ItemsPresenter.h"
#include "Layout/OverlayLayout.h"
#include "TreeInfrastructure.h"
#include "Window.h"

#include <stdexcept>
#include <utility>

ItemsPresenter::ItemsPresenter()
	: Control()
{
	RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	(void)TrySetPropertyValue(
		L"VerticalAlignment",
		BindingValue(::VerticalAlignment::Top),
		DependencyPropertyValueSource::Theme);
}

void ItemsPresenter::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	BeginRender();
	EndRender();
}

cui::core::Size ItemsPresenter::MeasureCore(
	const cui::core::Constraints& available)
{
	return cui::layout::MeasureOverlayChildren(
		GetLayoutChildrenView(), available);
}

void ItemsPresenter::Arrange(cui::core::Rect finalRect)
{
	Control::Arrange(finalRect);
	PerformPendingLayout();
}

void ItemsPresenter::RequestLayout()
{
	_contentLayoutPending = true;
	Control::RequestLayout();
}

void ItemsPresenter::OnComputedLayoutSizeChanged()
{
	_contentLayoutPending = true;
}

void ItemsPresenter::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_contentLayoutPending) return;
	const auto size = GetActualSizeDip();
	cui::layout::ArrangeOverlayChildren(
		GetLayoutChildrenView(),
		cui::core::Rect{ 0.0f, 0.0f, size.width, size.height });
	_contentLayoutPending = false;
}

void ItemsPresenter::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
}

bool ItemsPresenter::ValidateVisualChildCollection(
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

Panel* ItemsPresenter::SetItemsHost(std::unique_ptr<Panel> value)
{
	if (!value) throw std::invalid_argument("ItemsPresenter ItemsHost is null");
	if (_itemsHost)
		throw std::logic_error("ItemsPresenter already owns an ItemsHost");
	_itemsHost = value.get();
	_changingItemsHost = true;
	try
	{
		cui::framework::TreeAccess::AddOwnedVisualChild(
			*this, std::move(value), nullptr);
		_changingItemsHost = false;
	}
	catch (...)
	{
		_changingItemsHost = false;
		_itemsHost = nullptr;
		throw;
	}
	RequestLayout();
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
		detached = DetachVisualChild(previous);
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
