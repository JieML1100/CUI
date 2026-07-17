#pragma once

#include "DesignerModel/DesignDocument.h"
#include "DesignerPropertyValue.h"
#include <optional>
#include <string>
#include <vector>

/** Designer-facing description of one persisted form property. */
struct DesignerFormPropertyDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	std::wstring Category;
	int CategoryOrder = 1000;
	int Order = 0;
	DesignerStyleValueKind ValueKind = DesignerStyleValueKind::String;
	ControlPropertyEditorKind Editor = ControlPropertyEditorKind::Auto;
	DesignerStyleValue DefaultValue;
	std::optional<double> Minimum;
	std::optional<double> Maximum;
};

namespace DesignerFormPropertyCatalog
{
	/** Stable, presentation-ordered form property descriptors. */
	const std::vector<DesignerFormPropertyDescriptor>& GetProperties();

	const DesignerFormPropertyDescriptor* Find(const std::wstring& propertyName);

	/** Captures one effective value from the canonical design form model. */
	bool CaptureValue(
		const DesignerModel::DesignFormModel& form,
		const std::wstring& propertyName,
		DesignerStyleValue& out,
		std::wstring* outError = nullptr);

	/** Converts, coerces, and applies one value to the canonical form model. */
	bool ApplyValue(
		DesignerModel::DesignFormModel& form,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);

	/** Restores the descriptor's canonical default through the same Coerce path. */
	bool ResetValue(
		DesignerModel::DesignFormModel& form,
		const std::wstring& propertyName,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);
}
