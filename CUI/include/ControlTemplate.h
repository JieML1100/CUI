#pragma once

#include <memory>
#include <string>
#include <utility>

class Control;
enum class UIClass : int;

/**
 * Runtime factory for one fully initialized ControlTemplate instance.
 *
 * Apply instantiates a new visual tree on an existing control host. Build is
 * retained for generated item containers, which must construct a fresh host
 * and its template together as containers enter the viewport. Both paths keep
 * repeatable XAML construction independent from Designer document types.
 */
class IControlTemplate
{
public:
	virtual ~IControlTemplate() = default;
	virtual UIClass TargetType() const noexcept = 0;
	/**
	 * Instantiates this template on an existing behavior host.
	 *
	 * Control owns the ApplyTemplate lifecycle. Implementations only build and
	 * attach the XAML visual subtree to the supplied owner; they must never
	 * substitute a second host object.
	 */
	virtual bool Apply(
		Control& owner,
		std::wstring* outError = nullptr) const
	{
		(void)owner;
		if (outError)
			*outError =
				L"该 ControlTemplate 只支持容器构造，不支持现有宿主重套模板。";
		return false;
	}
	virtual std::unique_ptr<Control> Build(
		std::wstring* outError = nullptr) const = 0;
	/**
	 * Semantic equality keeps independently lowered references to the same
	 * XAML resource from invalidating an already materialized visual tree.
	 */
	virtual bool Equals(const IControlTemplate& other) const noexcept
	{
		return this == &other;
	}
};

/** Exact BindingValue payload used by repeatable runtime templates. */
class ControlTemplateReference final
{
public:
	ControlTemplateReference() = default;
	explicit ControlTemplateReference(
		std::shared_ptr<const IControlTemplate> value)
		: _value(std::move(value)) {}

	const IControlTemplate* Get() const noexcept { return _value.get(); }
	const std::shared_ptr<const IControlTemplate>& Shared() const noexcept
	{
		return _value;
	}
	explicit operator bool() const noexcept
	{
		return static_cast<bool>(_value);
	}
	bool operator==(const ControlTemplateReference& other) const noexcept
	{
		if (_value == other._value) return true;
		return _value && other._value && _value->Equals(*other._value);
	}

private:
	std::shared_ptr<const IControlTemplate> _value;
};
