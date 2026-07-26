#pragma once

#include "DesignerStyleSheet.h"
#include "../CUI/include/Style.h"
#include "../CUI/include/Resource.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace CuiRuntime { struct XamlTypePropertySchema; }
namespace DesignerModel
{
	struct DesignDocument;
	struct DesignItemsPanelTemplate;
}

namespace DesignerStyleSheetUtils
{
	using RuntimeStyleResource = std::pair<std::wstring, BindingValue>;
	std::vector<RuntimeStyleResource> BuildItemsPanelStyleResources(
		const std::vector<DesignerModel::DesignItemsPanelTemplate>& templates);
	std::vector<RuntimeStyleResource> BuildItemsPanelStyleResources(
		const DesignerModel::DesignDocument* document);

	std::wstring Trim(const std::wstring& value);
	std::wstring ValueKindName(DesignerStyleValueKind kind);
	bool TryParseValueKind(const std::wstring& value, DesignerStyleValueKind& out);
	std::vector<std::wstring> ValueKindNames();

	std::wstring UIClassName(UIClass type);
	bool TryParseUIClass(const std::wstring& value, UIClass& out);
	std::vector<std::wstring> UIClassNames(bool includeAny = true);


	bool TryConvertValue(
		const DesignerStyleValue& value,
		BindingValue& out,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	void Canonicalize(DesignerStyleSheet& styleSheet);
	/** WPF resource identity: x:Key for named Style, TargetType for implicit Style. */
	bool HasSameStyleResourceIdentity(
		const DesignerStyleRule& left,
		const DesignerStyleRule& right);
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
	using RulePropertySchemaResolver = std::function<bool(
		const DesignerStyleRule&,
		CuiRuntime::XamlTypePropertySchema&,
		std::wstring*)>;
	/** Validates built-in property existence, writability, type, and conversion. */
	bool ValidateAgainstPropertyMetadata(
		const DesignerStyleSheet& styleSheet,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	/** Rule-aware overload used when a component TargetType contributes metadata. */
	bool ValidateAgainstRulePropertyMetadata(
		const DesignerStyleSheet& styleSheet,
		const RulePropertySchemaResolver& schemaResolver,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	/**
	 * Lowers authored Begin/Pause/Resume/Stop storyboard actions through the
	 * same value/resource conversion used by dynamic and static Style paths.
	 */
	bool MaterializeStoryboardActions(
		const std::vector<DesignerEventTriggerAction>& sourceActions,
		const DesignerStyleSheet& styleSheet,
		std::vector<DeclarativeEventTriggerActionDefinition>& out,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {},
		const std::wstring& context = L"Style Trigger");
	bool BuildRuntimeStyleSheet(
		const DesignerStyleSheet& styleSheet,
		std::shared_ptr<ControlStyleSheet>& out,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {},
		const std::vector<RuntimeStyleResource>& supplementalResources = {});
}
