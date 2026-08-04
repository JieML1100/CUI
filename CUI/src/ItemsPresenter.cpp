#include "ItemsPresenter.h"
#include "Layout/OverlayLayout.h"
#include "TreeInfrastructure.h"
#include "Window.h"

#include <stdexcept>
#include <utility>

struct ItemsPresenter::ItemsHostMutationFrame final
{
	ItemsPresenter& Owner;
	ItemsHostMutationFrame* Previous = nullptr;
	ControlWeakReference ExpectedVisual;
	bool AllowsExpectedVisual = false;
	bool AllowsEmpty = false;

	ItemsHostMutationFrame(
		ItemsPresenter& owner,
		Control* expectedVisual,
		bool allowsExpectedVisual,
		bool allowsEmpty) noexcept
		: Owner(owner),
		Previous(owner._activeItemsHostMutation),
		ExpectedVisual(expectedVisual),
		AllowsExpectedVisual(allowsExpectedVisual),
		AllowsEmpty(allowsEmpty)
	{
		Owner._activeItemsHostMutation = this;
	}

	~ItemsHostMutationFrame()
	{
		Owner._activeItemsHostMutation = Previous;
	}

	ItemsHostMutationFrame(const ItemsHostMutationFrame&) = delete;
	ItemsHostMutationFrame& operator=(
		const ItemsHostMutationFrame&) = delete;
};

namespace
{
	bool OwnsExactItemsHostVisual(
		const ItemsPresenter& presenter,
		const Panel* itemsHost) noexcept
	{
		const auto children = presenter.GetVisualChildrenView();
		return itemsHost
			&& children.size() == 1
			&& children.front() == itemsHost
			&& itemsHost->GetVisualParent() == &presenter
			&& presenter.IndexOfVisualChild(itemsHost) == 0;
	}

	bool HasExactItemsHostEdges(
		const ItemsPresenter& presenter,
		const Panel* itemsHost,
		const ControlWeakReference& templatedParent,
		bool hadTemplatedParent) noexcept
	{
		auto* liveTemplatedParent = templatedParent.Get();
		return OwnsExactItemsHostVisual(presenter, itemsHost)
			&& (!hadTemplatedParent || liveTemplatedParent)
			&& presenter.GetTemplatedParent() == liveTemplatedParent
			&& itemsHost->GetTemplatedParent() == liveTemplatedParent
			&& itemsHost->GetLogicalParent() == nullptr;
	}
}

ItemsPresenter::ItemsPresenter()
	: Control()
{
	RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	(void)TrySetPropertyValue(
		Control::VerticalAlignmentProperty(),
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
	if (const auto* mutation = _activeItemsHostMutation)
	{
		auto* expected = mutation->ExpectedVisual.Get();
		const bool exactExpected = mutation->AllowsExpectedVisual
			&& expected
			&& children.size() == 1
			&& children.front() == expected;
		const bool exactEmpty =
			mutation->AllowsEmpty && children.empty();
		if (exactExpected || exactEmpty) return true;
		error = "ItemsPresenter ItemsHost transaction mismatch";
		return false;
	}
	if (!_itemsHost && children.empty()) return true;
	if (_itemsHost && children.size() == 1 && children.front() == _itemsHost)
		return true;
	error = "ItemsPresenter children are owned by its templated ItemsControl";
	return false;
}

Panel* ItemsPresenter::SetItemsHost(std::unique_ptr<Panel> value)
{
	if (!value) throw std::invalid_argument("ItemsPresenter ItemsHost is null");
	if (value.get() == _itemsHost)
	{
		(void)value.release();
		return _itemsHost;
	}
	if (_itemsHost)
		throw std::logic_error("ItemsPresenter already owns an ItemsHost");
	auto* candidate = value.get();
	const ControlWeakReference lifetime(candidate);
	auto* expectedTemplatedParent = GetTemplatedParent();
	const ControlWeakReference templatedParentLifetime(
		expectedTemplatedParent);
	const bool hadTemplatedParent = expectedTemplatedParent != nullptr;
	_itemsHost = candidate;
	ItemsHostMutationFrame mutation(
		*this, candidate, true, true);
	try
	{
		(void)cui::framework::TreeAccess::
			InsertOwnedVisualChildPreserving(
				*this, VisualChildCount(), value, nullptr);
	}
	catch (...)
	{
		auto* live = dynamic_cast<Panel*>(lifetime.Get());
		if (_itemsHost == candidate)
			_itemsHost = OwnsExactItemsHostVisual(*this, live)
				? live : nullptr;
		throw;
	}
	auto* live = dynamic_cast<Panel*>(lifetime.Get());
	if (!HasExactItemsHostEdges(
		*this, live, templatedParentLifetime,
		hadTemplatedParent))
	{
		if (_itemsHost == candidate)
			_itemsHost = OwnsExactItemsHostVisual(*this, live)
				? live : nullptr;
		throw std::logic_error(
			"ItemsPresenter ItemsHost attachment did not commit");
	}
	_itemsHost = live;
	RequestLayout();
	return _itemsHost;
}

std::unique_ptr<Panel> ItemsPresenter::DetachItemsHost()
{
	if (!_itemsHost) return {};
	auto* previous = _itemsHost;
	const ControlWeakReference lifetime(previous);
	auto* expectedTemplatedParent = GetTemplatedParent();
	const ControlWeakReference templatedParentLifetime(
		expectedTemplatedParent);
	const bool hadTemplatedParent = expectedTemplatedParent != nullptr;
	if (!HasExactItemsHostEdges(
		*this, previous, templatedParentLifetime,
		hadTemplatedParent))
		throw std::logic_error(
			"ItemsPresenter ItemsHost ownership is invalid");

	ItemsHostMutationFrame mutation(
		*this, nullptr, false, true);
	std::unique_ptr<Control> detached;
	bool ownershipCommit = false;
	std::exception_ptr notificationError;
	try
	{
		detached = cui::framework::TreeAccess::DetachVisualChild(
			*this, previous, &ownershipCommit,
			&notificationError);
	}
	catch (...)
	{
		auto* live = dynamic_cast<Panel*>(lifetime.Get());
		if (_itemsHost == previous)
			_itemsHost = OwnsExactItemsHostVisual(*this, live)
				? live : nullptr;
		throw;
	}
	auto* live = dynamic_cast<Panel*>(lifetime.Get());
	if (OwnsExactItemsHostVisual(*this, live))
	{
		_itemsHost = live;
		throw std::logic_error(
			"ItemsPresenter ItemsHost detach did not commit");
	}
	if (!GetVisualChildrenView().empty())
		throw std::logic_error(
			"ItemsPresenter retained an unexpected visual after ItemsHost detach");
	if (_itemsHost == previous)
		_itemsHost = nullptr;

	auto result = std::unique_ptr<Panel>(
		static_cast<Panel*>(detached.release()));
	if (!result)
		return {};
	if (result.get() != live
		|| result->GetVisualParent()
		|| IndexOfVisualChild(result.get()) >= 0
		|| result->GetLogicalParent()
		|| (!hadTemplatedParent
			? result->GetTemplatedParent() != nullptr
			: (!templatedParentLifetime.Get()
				|| result->GetTemplatedParent()
					!= templatedParentLifetime.Get())))
		throw std::logic_error(
			"ItemsPresenter detached ItemsHost ownership is invalid");
	return result;
}
