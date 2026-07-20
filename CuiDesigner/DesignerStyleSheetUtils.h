#pragma once

#include "DesignerStyleSheet.h"
#include "../CUI/include/Style.h"
#include "../CUI/include/Resource.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace DesignerStyleSheetUtils
{
	std::wstring Trim(const std::wstring& value);
	std::wstring ValueKindName(DesignerStyleValueKind kind);
	bool TryParseValueKind(const std::wstring& value, DesignerStyleValueKind& out);
	std::vector<std::wstring> ValueKindNames();

	std::wstring UIClassName(UIClass type);
	bool TryParseUIClass(const std::wstring& value, UIClass& out);
	std::vector<std::wstring> UIClassNames(bool includeAny = true);

	std::wstring FormatStates(ControlStyleState states);
	bool TryParseStates(const std::wstring& value, ControlStyleState& out);
	std::vector<std::wstring> SplitClasses(const std::wstring& value);
	std::wstring JoinClasses(const std::vector<std::wstring>& classes);
	std::wstring CanonicalTriggerProperty(const std::wstring& property);
	bool TryGetTriggerStates(
		const std::wstring& property,
		bool value,
		ControlStyleState& required,
		ControlStyleState& excluded);

	bool TryConvertValue(
		const DesignerStyleValue& value,
		BindingValue& out,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	void Canonicalize(DesignerStyleSheet& styleSheet);
	/** Appends one lexical dictionary, with nearer values shadowing outer ones. */
	void AppendLexicalScope(
		DesignerStyleSheet& target,
		const DesignerStyleSheet& source);
	/** Rewrites resource-bearing members of one Style rule. */
	void RemapRuleResourceKeys(
		DesignerStyleRule& rule,
		const std::vector<std::pair<std::wstring, std::wstring>>& renames,
		const std::function<bool(const std::wstring&)>& shouldRemap = {});
	/**
	 * Expands BasedOn chains into effective setters and inherited TargetType.
	 * The returned rules keep their selectors and source order, but BasedOn is
	 * cleared so every consumer observes the same resolved style definition.
	 */
	bool ResolveInheritance(
		const DesignerStyleSheet& styleSheet,
		DesignerStyleSheet& out,
		std::wstring* outError = nullptr);
	/**
	 * Resolves one authored local Style suffix against its visible lexical
	 * context and captures non-local StaticResource values into private aliases.
	 * DynamicResource keys remain lexical and are resolved by the target control.
	 */
	bool PrepareLocalRuntimeStyleSheet(
		const DesignerStyleSheet& localStyleSheet,
		const DesignerStyleSheet& visibleStyleSheet,
		DesignerStyleSheet& out,
		std::wstring* outError = nullptr);
	/** Resolves BasedOn and lowers Trigger/MultiTrigger/DataTrigger to selector rules. */
	bool ExpandRuntimeRules(
		const DesignerStyleSheet& styleSheet,
		DesignerStyleSheet& out,
		std::wstring* outError = nullptr);
	bool Validate(
		const DesignerStyleSheet& styleSheet,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	using ControlFactory = std::function<std::unique_ptr<Control>(UIClass)>;
	using RuleControlFactory = std::function<std::unique_ptr<Control>(
		const DesignerStyleRule&)>;
	/** Validates property existence, writability, type, conversion, and Coerce. */
	bool ValidateAgainstPropertyMetadata(
		const DesignerStyleSheet& styleSheet,
		const ControlFactory& controlFactory,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	/** Rule-aware overload used when a component TargetType needs dynamic metadata. */
	bool ValidateAgainstRulePropertyMetadata(
		const DesignerStyleSheet& styleSheet,
		const RuleControlFactory& controlFactory,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	bool BuildRuntimeStyleSheet(
		const DesignerStyleSheet& styleSheet,
		std::shared_ptr<ControlStyleSheet>& out,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
}
