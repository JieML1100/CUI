#pragma once

#include "DesignerTypes.h"

#include <functional>
#include <optional>
#include <string>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * One design-time event exposed by the property grid and understood by the
 * C++ generator. EventField and Signature are derived from the real runtime
 * event member. ParameterList is readable generated C++ text, not identity.
 */
struct DesignerEventDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	std::string EventField;
	/** C++ type that declares the real Event member; empty for portable custom events. */
	std::string EventOwnerTypeName;
	std::string ParameterList;
	DesignerEventCategory Category = DesignerEventCategory::Other;
	int Order = 0;
	/** The designer action activated by double-clicking the subject. */
	bool IsDefault = false;
	/** Exact Event function type; parameter names are deliberately excluded. */
	std::type_index Signature{ typeid(void) };

	DesignerEventDescriptor() = default;

	template<typename Owner, typename RuntimeEvent>
	static DesignerEventDescriptor FromEventMember(
		std::wstring name,
		std::string eventField,
		std::string parameterList,
		RuntimeEvent Owner::* eventMember)
	{
		using Member = RuntimeEvent Owner::*;
		using Function = typename RuntimeEvent::function_type;
		DesignerEventDescriptor result;
		result.Name = std::move(name);
		result.DisplayName = result.Name;
		result.EventField = std::move(eventField);
		result.ParameterList = std::move(parameterList);
		result.Signature = std::type_index(typeid(Function));
		result._memberType = std::type_index(typeid(Member));
		result._memberMatches = [eventMember](const void* candidate)
		{
			return candidate
				&& *static_cast<const Member*>(candidate) == eventMember;
		};
		return result;
	}

	template<typename Owner, typename RuntimeEvent>
	bool MatchesEventMember(RuntimeEvent Owner::* eventMember) const
	{
		using Member = RuntimeEvent Owner::*;
		using Function = typename RuntimeEvent::function_type;
		return Signature == std::type_index(typeid(Function))
			&& _memberType == std::type_index(typeid(Member))
			&& _memberMatches
			&& _memberMatches(&eventMember);
	}

	bool SameSignature(const DesignerEventDescriptor& other) const noexcept
	{
		return Signature == other.Signature;
	}

private:
	std::type_index _memberType{ typeid(void) };
	std::function<bool(const void*)> _memberMatches;
};

/** Shared event metadata and handler-name rules for the designer. */
class DesignerEventCatalog
{
public:
	static std::vector<DesignerEventDescriptor> GetControlEvents(UIClass type);
	static std::vector<DesignerEventDescriptor> GetControlEvents(
		UIClass type,
		const std::vector<DesignerComponentEventDescriptor>& componentEvents);
	static const std::vector<DesignerEventDescriptor>& GetWindowEvents();
	static std::optional<DesignerEventDescriptor> FindControlEvent(
		UIClass type, const std::wstring& eventName);
	static std::optional<DesignerEventDescriptor> FindControlEvent(
		UIClass type,
		const std::wstring& eventName,
		const std::vector<DesignerComponentEventDescriptor>& componentEvents);
	static std::optional<DesignerEventDescriptor> FindWindowEvent(
		const std::wstring& eventName);
	static std::optional<DesignerEventDescriptor> GetDefaultControlEvent(
		UIClass type);
	static std::optional<DesignerEventDescriptor> GetDefaultControlEvent(
		UIClass type,
		const std::vector<DesignerComponentEventDescriptor>& componentEvents);
	static std::optional<DesignerEventDescriptor> GetDefaultWindowEvent();
	static const wchar_t* GetCategoryDisplayName(
		DesignerEventCategory category) noexcept;
	static const char* GetCategoryName(
		DesignerEventCategory category) noexcept;
	static bool TryParseCategory(
		const std::wstring& value,
		DesignerEventCategory& category) noexcept;
	static const char* GetComponentPayloadName(
		DesignerComponentEventPayload payload) noexcept;
	static bool TryParseComponentPayload(
		const std::wstring& value,
		DesignerComponentEventPayload& payload) noexcept;
	static const char* GetComponentRoutingStrategyName(
		DeclarativeEventRoutingStrategy strategy) noexcept;
	static bool TryParseComponentRoutingStrategy(
		const std::wstring& value,
		DeclarativeEventRoutingStrategy& strategy) noexcept;
	/** Stable document key for an attached routed-component event handler. */
	static std::wstring MakeAttachedComponentEventKey(
		const DesignerComponentType& ownerType,
		const std::wstring& eventName);
	static bool TryParseAttachedComponentEventKey(
		const std::wstring& key,
		DesignerComponentType& ownerType,
		std::wstring& eventName) noexcept;
	static bool ValidateComponentEvents(
		UIClass baseType,
		const std::vector<DesignerComponentEventDescriptor>& events,
		std::wstring* outError = nullptr);
	static bool IsKnownEventName(const std::wstring& eventName);

	static std::optional<DesignerEventDescriptor> FromComponentEvent(
		const DesignerComponentEventDescriptor& event) noexcept;

	static std::wstring MakeDefaultHandlerName(
		const std::wstring& subjectName, const std::wstring& eventName);
	/** Canonicalizes an explicitly authored C++ handler identifier. */
	static std::wstring NormalizeHandlerName(
		const std::wstring& storedValue);

	/** Empty is valid and means unbound. Non-empty values must be C++ identifiers. */
	static bool ValidateHandlerName(const std::wstring& handlerName,
		std::wstring* error = nullptr);
};
