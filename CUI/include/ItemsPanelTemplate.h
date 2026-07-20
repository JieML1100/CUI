#pragma once

#include "Layout/LayoutTypes.h"

#include <memory>
#include <utility>

/** Layout host selected by an ItemsPanelTemplate resource. */
enum class ItemsPanelKind : int
{
	Stack = 0,
	Wrap = 1,
	VirtualizingStack = 2
};

/**
 * Immutable runtime projection of an XAML ItemsPanelTemplate.
 *
 * VirtualizingStack currently requires a positive ItemHeight so extent,
 * hit testing and BringIntoView remain exact instead of estimate-driven.
 * CacheLength is expressed in viewport lengths.
 */
struct ItemsPanelTemplate final
{
	ItemsPanelKind Kind = ItemsPanelKind::Stack;
	Orientation Orientation = Orientation::Vertical;
	float Spacing = 0.0f;
	float ItemWidth = 0.0f;
	float ItemHeight = 0.0f;
	float CacheLength = 1.0f;

	bool operator==(const ItemsPanelTemplate&) const = default;
};

/** Strong BindingValue payload used by ItemsControl.ItemsPanel. */
class ItemsPanelTemplateReference final
{
public:
	ItemsPanelTemplateReference() = default;
	explicit ItemsPanelTemplateReference(
		std::shared_ptr<const ItemsPanelTemplate> value)
		: _value(std::move(value)) {}

	const ItemsPanelTemplate* Get() const noexcept { return _value.get(); }
	const std::shared_ptr<const ItemsPanelTemplate>& Shared() const noexcept
	{
		return _value;
	}
	explicit operator bool() const noexcept
	{
		return static_cast<bool>(_value);
	}
	bool operator==(const ItemsPanelTemplateReference& other) const noexcept
	{
		return _value == other._value;
	}

private:
	std::shared_ptr<const ItemsPanelTemplate> _value;
};
