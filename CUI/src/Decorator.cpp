#include "Decorator.h"

#include "Layout/OverlayLayout.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

void Decorator::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
}

Control* Decorator::GetChild() const noexcept
{
	const auto children = GetVisualChildrenView();
	return children.size() == 1 ? children.front() : nullptr;
}

Control* Decorator::SetChild(std::unique_ptr<Control> value)
{
	if (value.get() == GetChild()) return value.release();
	auto previous = DetachChild();
	if (!value) return nullptr;
	try
	{
		return AddOwned(std::move(value));
	}
	catch (...)
	{
		if (previous) AddOwned(std::move(previous));
		throw;
	}
}

bool Decorator::TrySetChild(std::unique_ptr<Control>& value) noexcept
{
	if (!value || GetChild()) return false;
	auto* raw = value.get();
	try
	{
		InsertVisualChild(0, raw);
		value.release();
		return true;
	}
	catch (...)
	{
		if (raw->GetVisualParent() == this)
		{
			try { value = DetachVisualChild(raw); }
			catch (...) {}
		}
		return false;
	}
}

std::unique_ptr<Control> Decorator::DetachChild()
{
	auto* child = GetChild();
	return child ? DetachVisualChild(child) : std::unique_ptr<Control>{};
}

cui::core::Size Decorator::MeasureCore(
	const cui::core::Constraints& available)
{
	return cui::layout::MeasureOverlayChildren(
		GetLayoutChildrenView(), available, GetDecoratorInsets());
}

void Decorator::Arrange(cui::core::Rect finalRect)
{
	Control::Arrange(finalRect);
	PerformPendingLayout();
}

cui::core::Point Decorator::GetVisualChildrenLayoutOriginDip()
{
	const auto insets = GetDecoratorInsets();
	return { insets.left, insets.top };
}

void Decorator::RequestLayout()
{
	_childLayoutPending = true;
	Control::RequestLayout();
}

void Decorator::OnComputedLayoutSizeChanged()
{
	_childLayoutPending = true;
}

void Decorator::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_childLayoutPending) return;
	const auto size = GetActualSizeDip();
	const auto insets = GetDecoratorInsets();
	cui::layout::ArrangeOverlayChildren(
		GetLayoutChildrenView(),
		cui::core::Rect{
			insets.left,
			insets.top,
			(std::max)(0.0f, size.width - insets.Horizontal()),
			(std::max)(0.0f, size.height - insets.Vertical()) });
	_childLayoutPending = false;
}

bool Decorator::ValidateVisualChildCollection(
	std::span<Control* const> children,
	std::string& error) const
{
	const auto count = std::count_if(
		children.begin(), children.end(),
		[](const Control* child) { return child != nullptr; });
	if (count <= 1) return true;
	error = "Decorator accepts exactly one optional Child";
	return false;
}

void Decorator::OnVisualChildCollectionChanged(
	const CollectionChangedEventArgs& change,
	std::span<Control* const> previousChildren)
{
	(void)change;
	(void)previousChildren;
	_childLayoutPending = true;
}
