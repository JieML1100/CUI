#pragma once

#include "DesignerStyleSheet.h"
#include "../CUI/include/Style.h"
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

	bool TryConvertValue(
		const DesignerStyleValue& value,
		BindingValue& out,
		std::wstring* outError = nullptr);
	void Canonicalize(DesignerStyleSheet& styleSheet);
	bool Validate(const DesignerStyleSheet& styleSheet, std::wstring* outError = nullptr);
	using ControlFactory = std::function<std::unique_ptr<Control>(UIClass)>;
	/** Validates property existence, writability, type, conversion, and Coerce. */
	bool ValidateAgainstPropertyMetadata(
		const DesignerStyleSheet& styleSheet,
		const ControlFactory& controlFactory,
		std::wstring* outError = nullptr);
	bool BuildRuntimeStyleSheet(
		const DesignerStyleSheet& styleSheet,
		std::shared_ptr<ControlStyleSheet>& out,
		std::wstring* outError = nullptr);
}
