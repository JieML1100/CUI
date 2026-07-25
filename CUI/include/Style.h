#pragma once

#include "Control.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/** A literal metadata value or a late-bound resource reference. */
struct ControlStyleValue
{
	BindingValue Literal;
	std::wstring ResourceKey;
	bool IsDynamicResource = false;

	ControlStyleValue() = default;
	explicit ControlStyleValue(BindingValue value)
		: Literal(std::move(value)) {}

	static ControlStyleValue Resource(std::wstring key)
	{
		ControlStyleValue result;
		result.ResourceKey = std::move(key);
		return result;
	}

	static ControlStyleValue DynamicResource(std::wstring key)
	{
		ControlStyleValue result;
		result.ResourceKey = std::move(key);
		result.IsDynamicResource = true;
		return result;
	}

	bool IsResource() const noexcept { return !ResourceKey.empty(); }
};

/** Assigns one metadata property when its containing Setter or Trigger is active. */
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

	static ControlStyleSetter DynamicResource(
		std::wstring propertyName,
		std::wstring resourceKey)
	{
		return ControlStyleSetter(
			std::move(propertyName),
			ControlStyleValue::DynamicResource(std::move(resourceKey)));
	}
};

/** One observable DataContext path/value predicate used by DataTrigger. */
struct ControlStyleDataCondition
{
	std::wstring SourceProperty;
	BindingValue Value;
};

/** One target-property/value predicate used by Trigger or MultiTrigger. */
struct ControlStylePropertyCondition
{
	std::wstring PropertyName;
	BindingValue Value;
};

/**
 * One lowered WPF Style rule. The resource key and exact target type select a
 * single effective Style; conditions only select Setter/Trigger entries inside
 * that Style and never participate in cross-style cascading.
 */
struct ControlStyleSelector
{
	std::optional<UIClass> Type;
	/** Exact XAML component identity; empty means no component-type constraint. */
	std::wstring DeclarativeTypeNamespace;
	std::wstring DeclarativeTypeName;
	std::wstring StyleResourceKey;
	std::vector<ControlStylePropertyCondition> PropertyConditions;
	std::vector<ControlStyleDataCondition> DataConditions;

	bool MatchesTargetType(Control& target) const;
	bool MatchesConditions(Control& target) const;
	bool IsConditional() const noexcept;
};

struct ControlStyleRule
{
	size_t Id = 0;
	ControlStyleSelector Selector;
	std::vector<ControlStyleSetter> Setters;
	/** WPF TriggerBase edge actions; clocks are instantiated per target control. */
	std::vector<DeclarativeEventTriggerActionDefinition> EnterActions;
	std::vector<DeclarativeEventTriggerActionDefinition> ExitActions;
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
	std::wstring ResourceKey;
	bool IsDynamicResource = false;
	size_t RuleId = 0;
	bool IsConditional = false;
};

struct ResolvedControlStyleTrigger
{
	size_t RuleId = 0;
	bool IsActive = false;
	std::vector<DeclarativeEventTriggerActionDefinition> EnterActions;
	std::vector<DeclarativeEventTriggerActionDefinition> ExitActions;
};

struct ControlStyleResolution
{
	bool HasStyle = false;
	std::vector<ResolvedControlStyleSetter> Setters;
	/** Includes active and inactive action rules so targets can detect edges. */
	std::vector<ResolvedControlStyleTrigger> Triggers;
	std::vector<ControlStyleResolutionIssue> Issues;

	bool Success() const noexcept { return Issues.empty(); }
};

/**
 * Observable control-level collection of lowered WPF Style resources.
 * Attached controls refresh automatically whenever rules or resources change.
 * Data conditions always resolve through each target's effective DataContext;
	 * the shared sheet never owns or supplies a context.
 */
class ControlStyleSheet final
{
public:
	ControlStyleSheet();
	~ControlStyleSheet();
	ControlStyleSheet(const ControlStyleSheet&) = delete;
	ControlStyleSheet& operator=(const ControlStyleSheet&) = delete;

	size_t AddRule(
		ControlStyleSelector selector,
		std::vector<ControlStyleSetter> setters,
		std::vector<DeclarativeEventTriggerActionDefinition> enterActions = {},
		std::vector<DeclarativeEventTriggerActionDefinition> exitActions = {});
	bool RemoveRule(size_t ruleId);
	void ClearRules();
	const std::vector<ControlStyleRule>& Rules() const noexcept { return _rules; }

	bool SetResource(std::wstring key, BindingValue value);
	bool RemoveResource(const std::wstring& key);
	void ClearResources();
	bool TryGetResource(const std::wstring& key, BindingValue& value) const;

	ControlStyleResolution Resolve(
		Control& target,
		bool themeStyle = false) const;
	bool UsesPropertyCondition(const std::wstring& propertyName) const;
	/** Unique DataTrigger paths used to build target-local DataContext observers. */
	std::vector<std::wstring> DataConditionPaths() const;
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
	mutable uint64_t _revision = 0;
	mutable Event<void()> _changed;

	bool MatchesDataConditions(
		const ControlStyleSelector& selector,
		Control& target) const;
	void NotifyChanged() const;
};
