#pragma once

#include "DesignerControlPropertyCatalog.h"
#include "DesignerFormPropertyCatalog.h"
#include "DesignerPropertyCatalog.h"
#include <optional>
#include <string>
#include <vector>

enum class DesignerPropertyRowSource : unsigned char
{
	Form,
	ControlDesign,
	CustomDescriptor,
	RuntimeMetadata
};

enum class DesignerPropertyRowEditorKind : unsigned char
{
	Text,
	Boolean,
	Choice,
	Color,
	Thickness,
	FloatSlider,
	FontName,
	FontSize,
	Anchor
};

enum class DesignerPropertyDiagnosticKind : unsigned char
{
	Binding,
	Validation,
	Style,
	Theme
};

struct DesignerPropertyDiagnostic
{
	DesignerPropertyDiagnosticKind Kind = DesignerPropertyDiagnosticKind::Binding;
	BindingValidationSeverity Severity = BindingValidationSeverity::Info;
	std::wstring Summary;
	std::wstring Details;

	bool operator==(const DesignerPropertyDiagnostic&) const = default;
};

/** Presentation-neutral row consumed by PropertyGrid and other Designer surfaces. */
struct DesignerPropertyRow
{
	struct Choice
	{
		std::wstring DisplayName;
		std::wstring ValueText;
	};

	DesignerPropertyRowSource Source = DesignerPropertyRowSource::RuntimeMetadata;
	std::wstring Name;
	std::wstring DisplayName;
	std::wstring Category;
	int CategoryOrder = 1000;
	int Order = 0;
	DesignerStyleValue Value;
	DesignerPropertyRowEditorKind Editor = DesignerPropertyRowEditorKind::Text;
	std::vector<Choice> Choices;
	std::optional<double> Minimum;
	std::optional<double> Maximum;
	std::optional<double> Step;
	std::optional<ControlPropertyValueSource> EffectiveValueSource;
	std::vector<DesignerPropertyDiagnostic> Diagnostics;
	bool HasMixedValue = false;
	bool HasMixedValueSource = false;
	bool HasConfiguredBinding = false;
	bool HasMixedDiagnostics = false;
	bool IsReadOnly = false;
	bool CanReset = false;
};

namespace DesignerPropertyRowCatalog
{
	/** Projects the canonical form model into presentation-ordered rows. */
	std::vector<DesignerPropertyRow> GetFormRows(
		const DesignerModel::DesignFormModel& form);

	/**
	 * Merges wrapper-owned and runtime-metadata properties, removes duplicate
	 * names in favor of the wrapper contract, and returns one global order.
	 */
	std::vector<DesignerPropertyRow> GetControlRows(
		DesignerControl& target,
		const DesignerControlPropertyContext& context);

	/**
	 * Intersects already-projected control rows for a multi-selection. Identity
	 * properties are omitted; compatible shared rows keep the primary row's
	 * presentation and report mixed values/value sources explicitly.
	 */
	std::vector<DesignerPropertyRow> GetCommonControlRows(
		const std::vector<std::vector<DesignerPropertyRow>>& controlRows);

	const DesignerPropertyRow* Find(
		const std::vector<DesignerPropertyRow>& rows,
		const std::wstring& propertyName);

	/** Case-insensitive whitespace-token AND matching used by Designer filters. */
	bool MatchesFilterText(
		const std::wstring& searchableText,
		const std::wstring& filterText);

	/** Preserves row order while filtering names, category, value, choices, and sources. */
	std::vector<DesignerPropertyRow> FilterRows(
		const std::vector<DesignerPropertyRow>& rows,
		const std::wstring& filterText);
}
