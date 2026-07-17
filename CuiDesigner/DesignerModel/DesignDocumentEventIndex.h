#pragma once

#include "DesignDocument.h"

#include <optional>
#include <string>
#include <typeindex>
#include <vector>

namespace DesignerModel
{
enum class DesignEventOwnerKind
{
	Form,
	Control,
};

/** One resolved event-to-handler reference in a design document. */
struct DesignEventReference
{
	DesignEventOwnerKind OwnerKind = DesignEventOwnerKind::Control;
	int OwnerDesignId = 0;
	std::wstring SubjectName;
	UIClass SubjectType = UIClass::UI_Base;
	std::wstring EventName;
	std::string EventField;
	std::string ParameterList;
	std::type_index Signature{ typeid(void) };
	std::wstring HandlerName;
	bool UsedConventionalName = false;
};

/** All references sharing one C++ member-function name and signature. */
struct DesignEventHandlerEntry
{
	std::wstring Name;
	std::string ParameterList;
	std::type_index Signature{ typeid(void) };
	std::vector<size_t> ReferenceIndices;
};

/**
 * Derived, deterministic index of the event handler contract in a document.
 * It validates unknown events, invalid identifiers, and cross-signature reuse.
 */
class DesignDocumentEventIndex final
{
public:
	static bool Build(
		const DesignDocument& document,
		DesignDocumentEventIndex& output,
		std::wstring* outError = nullptr);

	const std::vector<DesignEventReference>& References() const noexcept
	{
		return _references;
	}
	const std::vector<DesignEventHandlerEntry>& Handlers() const noexcept
	{
		return _handlers;
	}
	const DesignEventHandlerEntry* FindHandler(
		const std::wstring& name) const noexcept;

	/**
	 * Renames every reference transactionally. Reusing an existing name is
	 * allowed only when its exact Event function type matches.
	 */
	static bool RenameHandler(
		DesignDocument& document,
		const std::wstring& oldName,
		const std::wstring& newName,
		size_t* outRenamedReferenceCount = nullptr,
		std::wstring* outError = nullptr);

private:
	std::vector<DesignEventReference> _references;
	std::vector<DesignEventHandlerEntry> _handlers;
};
}
