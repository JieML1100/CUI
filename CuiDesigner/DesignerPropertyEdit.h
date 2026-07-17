#pragma once

#include "DesignerControlPropertyCatalog.h"
#include "DesignerPropertyRowCatalog.h"
#include <cstddef>
#include <string>
#include <vector>

struct DesignerPropertyEditTarget
{
	DesignerControl* Control = nullptr;
	DesignerControlPropertyContext Context;
};

/** Minimal reversible state for one property on one control. */
struct DesignerPropertyValueSnapshot
{
	DesignerStyleValue DesignValue;
	bool HasLocalValue = false;
	BindingValue LocalValue;
	DesignerStyleValue LocalSerializedValue;
	DesignerStyleValue EffectiveSerializedValue;
	bool UsesLegacyPersistence = false;
	bool HasTrackedValue = false;
	DesignerStyleValue TrackedValue;
	std::wstring CanonicalPropertyName;

	bool EquivalentTo(const DesignerPropertyValueSnapshot& other) const noexcept;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

struct DesignerPropertyTargetSnapshot
{
	std::wstring TargetName;
	UIClass TargetType = UIClass::UI_Base;
	DesignerPropertyValueSnapshot Value;

	bool EquivalentTo(const DesignerPropertyTargetSnapshot& other) const noexcept;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

struct DesignerPropertyBatchSnapshot
{
	DesignerPropertyRowSource Source =
		DesignerPropertyRowSource::RuntimeMetadata;
	std::wstring PropertyName;
	std::vector<DesignerPropertyTargetSnapshot> Targets;

	bool EquivalentTo(const DesignerPropertyBatchSnapshot& other) const noexcept;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

struct DesignerPropertyEditResult
{
	bool Succeeded = false;
	size_t AppliedCount = 0;
	std::wstring Error;

	explicit operator bool() const noexcept { return Succeeded; }

	static DesignerPropertyEditResult Success(size_t appliedCount = 0);
	static DesignerPropertyEditResult Failure(
		std::wstring error,
		size_t appliedCount = 0);
};

namespace DesignerPropertyEdit
{
	/** Captures only the state required to restore one property. */
	bool CaptureSnapshot(
		const DesignerPropertyEditTarget& target,
		const DesignerPropertyRow& row,
		DesignerPropertyValueSnapshot& snapshot,
		std::wstring* outError = nullptr);

	/** Restores one captured property state through the canonical catalog path. */
	bool RestoreSnapshot(
		const DesignerPropertyEditTarget& target,
		const DesignerPropertyRow& row,
		const DesignerPropertyValueSnapshot& snapshot,
		std::wstring* outError = nullptr);

	/** Validates every target without mutating it. */
	DesignerPropertyEditResult Validate(
		const std::vector<DesignerPropertyEditTarget>& targets,
		const DesignerPropertyRow& row,
		const std::wstring& valueText);

	/** Validates first, then applies the same typed value to every target. */
	DesignerPropertyEditResult Apply(
		const std::vector<DesignerPropertyEditTarget>& targets,
		const DesignerPropertyRow& row,
		const std::wstring& valueText);

	/** Resets every target through the same catalog that produced the row. */
	DesignerPropertyEditResult Reset(
		const std::vector<DesignerPropertyEditTarget>& targets,
		const DesignerPropertyRow& row);
}
