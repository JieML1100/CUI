#pragma once

#include <memory>
#include <string>
#include <utility>

class Control;
enum class UIClass : int;

/**
 * Runtime factory for one fully initialized ControlTemplate host.
 *
 * Most document controls are expanded once while a document is materialized.
 * Virtualized item containers are different: they must instantiate the same
 * template repeatedly as containers enter the viewport. This interface keeps
 * that repeatable construction independent from Designer document types.
 */
class IControlTemplate
{
public:
	virtual ~IControlTemplate() = default;
	virtual UIClass TargetType() const noexcept = 0;
	virtual std::unique_ptr<Control> Build(
		std::wstring* outError = nullptr) const = 0;
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
		return _value == other._value;
	}

private:
	std::shared_ptr<const IControlTemplate> _value;
};
