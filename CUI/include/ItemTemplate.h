#pragma once

#include "BindingList.h"

#include <memory>
#include <string>
#include <functional>

class Control;

/** Runtime visual factory for one strongly typed binding record. */
class IItemTemplate
{
public:
	virtual ~IItemTemplate() = default;
	virtual const std::wstring& DataTypeName() const noexcept = 0;
	virtual std::unique_ptr<Control> Build(
		const BindingSourceReference& item,
		size_t index,
		std::wstring* outError = nullptr) const = 0;
	/** True when the template also supplies a child ItemsSource for a tree item. */
	virtual bool IsHierarchical() const noexcept { return false; }
	/** Evaluates HierarchicalDataTemplate.ItemsSource against one data item. */
	virtual bool TryGetChildItemsSource(
		const BindingSourceReference& item,
		BindingListReference& out,
		std::wstring* outError = nullptr) const
	{
		(void)item;
		out = {};
		if (outError) outError->clear();
		return true;
	}
	/** Observes the source path that produces the child list. */
	virtual BindingPathObservation ObserveChildItemsSource(
		const BindingSourceReference& item,
		std::function<void()> changed) const
	{
		(void)item;
		(void)changed;
		return {};
	}
};

/** Exact BindingValue payload used by ItemsControl.ItemTemplate. */
class ItemTemplateReference final
{
public:
	ItemTemplateReference() = default;
	explicit ItemTemplateReference(std::shared_ptr<const IItemTemplate> value)
		: _value(std::move(value)) {}

	const IItemTemplate* Get() const noexcept { return _value.get(); }
	const std::shared_ptr<const IItemTemplate>& Shared() const noexcept
	{
		return _value;
	}
	explicit operator bool() const noexcept { return static_cast<bool>(_value); }
	bool operator==(const ItemTemplateReference& other) const noexcept
	{
		return _value == other._value;
	}

private:
	std::shared_ptr<const IItemTemplate> _value;
};
