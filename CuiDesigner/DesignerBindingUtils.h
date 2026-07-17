#pragma once

#include "DesignerTypes.h"
#include <string>

namespace DesignerBindingUtils
{
	struct TargetMetadata
	{
		std::wstring Name;
		BindingValueKind ValueKind = BindingValueKind::Empty;
		bool CanRead = false;
		bool CanWrite = false;
		bool CanObserve = false;
	};

	std::wstring Trim(const std::wstring& value);
	bool IsValidSourcePath(const std::wstring& path);

	const wchar_t* BindingModeName(BindingMode mode) noexcept;
	bool TryParseBindingMode(const std::wstring& value, BindingMode& mode);
	const wchar_t* UpdateModeName(DataSourceUpdateMode mode) noexcept;
	bool TryParseUpdateMode(const std::wstring& value, DataSourceUpdateMode& mode);
	const wchar_t* ValueKindName(BindingValueKind kind) noexcept;

	bool IsModeStructurallyCompatible(
		const BindingPropertyMetadata& metadata,
		BindingMode mode) noexcept;
	bool IsCompatible(
		const BindingPropertyMetadata& metadata,
		const DesignerDataBinding& binding) noexcept;
	bool IsModeStructurallyCompatible(
		const TargetMetadata& metadata,
		BindingMode mode) noexcept;

	/** Validates a portable design-time target without requiring runtime registration. */
	bool ValidateTarget(
		const TargetMetadata& target,
		const DesignerDataBinding& binding,
		std::wstring* outError = nullptr,
		const DesignerDataContextSchema* sourceSchema = nullptr);

	bool Validate(
		Control& target,
		const std::wstring& targetProperty,
		const DesignerDataBinding& binding,
		const BindingPropertyMetadata** outMetadata = nullptr,
		std::wstring* outError = nullptr,
		const DesignerDataContextSchema* sourceSchema = nullptr);

	std::wstring Describe(
		const std::wstring& targetProperty,
		const DesignerDataBinding& binding);
}
