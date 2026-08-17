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
#if CUI_ENABLE_DYNAMIC_XAML
	virtual DataTypeToken GetDataTypeToken() const noexcept
	{
		return MakeDataTypeToken(DataTypeName());
	}
	virtual const std::wstring& DataTypeName() const noexcept = 0;
#else
	virtual DataTypeToken GetDataTypeToken() const noexcept = 0;
#endif
	virtual std::unique_ptr<Control> Build(
		const BindingSourceReference& item,
		size_t index,
		std::wstring* outError = nullptr) const = 0;
	/** True when the template also supplies a child ItemsSource for a tree item. */
	virtual bool IsHierarchical() const noexcept { return false; }
	/** Evaluates HierarchicalDataTemplate.ItemsSource against one data item. */
	virtual bool TryGetVisualChildItemsSource(
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

/**
 * WPF-shaped programmable template policy.
 *
 * The selector is shared by all realized containers. Implementations must not
 * retain the supplied container; virtualization can recycle or destroy it as
 * soon as SelectTemplate returns.
 */
class IItemTemplateSelector
{
public:
	virtual ~IItemTemplateSelector() = default;
	virtual ItemTemplateReference SelectTemplate(
		const BindingSourceReference& item,
		Control& container) const = 0;
};

/** Shared immutable selector identity used by template-bearing controls. */
class ItemTemplateSelectorReference final
{
public:
	ItemTemplateSelectorReference() = default;
	explicit ItemTemplateSelectorReference(
		std::shared_ptr<const IItemTemplateSelector> value)
		: _value(std::move(value)) {}

	const IItemTemplateSelector* Get() const noexcept { return _value.get(); }
	const std::shared_ptr<const IItemTemplateSelector>& Shared() const noexcept
	{
		return _value;
	}
	explicit operator bool() const noexcept
	{
		return static_cast<bool>(_value);
	}
	bool operator==(const ItemTemplateSelectorReference& other) const noexcept
	{
		return _value == other._value;
	}

private:
	std::shared_ptr<const IItemTemplateSelector> _value;
};

/**
 * WPF-shaped programmable item-container style policy.
 *
 * CUI styles are addressed by resource key, so the applicable native subset
 * returns that key instead of copying a mutable Style graph into every
 * container.  The selector is shared and must not retain the recyclable
 * container supplied to SelectStyle.
 */
class IItemStyleSelector
{
public:
	virtual ~IItemStyleSelector() = default;
	virtual std::wstring SelectStyle(
		const BindingSourceReference& item,
		Control& container) const = 0;
};

/** Shared immutable style-selector identity used by item controls. */
class ItemStyleSelectorReference final
{
public:
	ItemStyleSelectorReference() = default;
	explicit ItemStyleSelectorReference(
		std::shared_ptr<const IItemStyleSelector> value)
		: _value(std::move(value)) {}

	const IItemStyleSelector* Get() const noexcept { return _value.get(); }
	const std::shared_ptr<const IItemStyleSelector>& Shared() const noexcept
	{
		return _value;
	}
	explicit operator bool() const noexcept
	{
		return static_cast<bool>(_value);
	}
	bool operator==(const ItemStyleSelectorReference& other) const noexcept
	{
		return _value == other._value;
	}

private:
	std::shared_ptr<const IItemStyleSelector> _value;
};
