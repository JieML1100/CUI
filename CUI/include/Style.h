#pragma once

#include "Control.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/** A literal metadata value or a late-bound resource reference. */
struct ControlStyleValue
{
	BindingValue Literal;
	std::wstring ResourceKey;

	ControlStyleValue() = default;
	explicit ControlStyleValue(BindingValue value)
		: Literal(std::move(value)) {}

	static ControlStyleValue Resource(std::wstring key)
	{
		ControlStyleValue result;
		result.ResourceKey = std::move(key);
		return result;
	}

	bool IsResource() const noexcept { return !ResourceKey.empty(); }
};

/** Assigns one metadata property when its containing rule wins the cascade. */
struct ControlStyleSetter
{
	std::wstring PropertyName;
	ControlStyleValue Value;

	ControlStyleSetter() = default;
	ControlStyleSetter(std::wstring propertyName, BindingValue value)
		: PropertyName(std::move(propertyName)), Value(std::move(value)) {}
	ControlStyleSetter(std::wstring propertyName, ControlStyleValue value)
		: PropertyName(std::move(propertyName)), Value(std::move(value)) {}

	static ControlStyleSetter Resource(
		std::wstring propertyName,
		std::wstring resourceKey)
	{
		return ControlStyleSetter(
			std::move(propertyName),
			ControlStyleValue::Resource(std::move(resourceKey)));
	}
};

/**
 * Matches a control by runtime type, style id, all listed classes, and states.
 * UIClass::UI_Base is a wildcard type selector; any other type is exact.
 */
struct ControlStyleSelector
{
	std::optional<UIClass> Type;
	std::wstring Id;
	std::vector<std::wstring> Classes;
	ControlStyleState RequiredStates = ControlStyleState::None;
	ControlStyleState ExcludedStates = ControlStyleState::None;

	bool Matches(Control& target) const;
	uint32_t Specificity() const noexcept;
};

struct ControlStyleRule
{
	size_t Id = 0;
	ControlStyleSelector Selector;
	std::vector<ControlStyleSetter> Setters;
};

enum class ControlStyleResolutionIssueCode
{
	MissingResource,
	PropertyNotFound,
	PropertyNotWritable,
	InvalidValue
};

struct ControlStyleResolutionIssue
{
	ControlStyleResolutionIssueCode Code =
		ControlStyleResolutionIssueCode::MissingResource;
	size_t RuleId = 0;
	std::wstring PropertyName;
	std::wstring ResourceKey;
};

struct ResolvedControlStyleSetter
{
	std::wstring PropertyName;
	BindingValue Value;
	size_t RuleId = 0;
	uint32_t Specificity = 0;
};

struct ControlStyleResolution
{
	std::vector<ResolvedControlStyleSetter> Setters;
	std::vector<ControlStyleResolutionIssue> Issues;

	bool Success() const noexcept { return Issues.empty(); }
};

/**
 * Observable control-level style sheet with CSS-like selector specificity.
 * Attached controls refresh automatically whenever rules or resources change.
 */
class ControlStyleSheet final
{
public:
	ControlStyleSheet() = default;
	ControlStyleSheet(const ControlStyleSheet&) = delete;
	ControlStyleSheet& operator=(const ControlStyleSheet&) = delete;

	size_t AddRule(
		ControlStyleSelector selector,
		std::vector<ControlStyleSetter> setters);
	bool RemoveRule(size_t ruleId);
	void ClearRules();
	const std::vector<ControlStyleRule>& Rules() const noexcept { return _rules; }

	bool SetResource(std::wstring key, BindingValue value);
	bool RemoveResource(const std::wstring& key);
	void ClearResources();
	bool TryGetResource(const std::wstring& key, BindingValue& value) const;

	ControlStyleResolution Resolve(Control& target) const;
	uint64_t Revision() const noexcept { return _revision; }
	EventConnection SubscribeChanged(std::function<void()> handler) const;

private:
	struct ResourceEntry
	{
		std::wstring Key;
		BindingValue Value;
	};

	std::vector<ControlStyleRule> _rules;
	std::vector<ResourceEntry> _resources;
	size_t _nextRuleId = 1;
	uint64_t _revision = 0;
	mutable Event<void()> _changed;

	void NotifyChanged();
};
