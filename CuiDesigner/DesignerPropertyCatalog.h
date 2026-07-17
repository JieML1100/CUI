#pragma once

#include "DesignerStyleSheet.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

/**
 * Designer-facing projection of one writable runtime property. The runtime
 * metadata remains authoritative; this descriptor only adds an editable
 * literal kind and a representative text value.
 */
struct DesignerPropertyDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	std::wstring Category;
	int CategoryOrder = 1000;
	int Order = 0;
	DesignerStyleValueKind ValueKind = DesignerStyleValueKind::String;
	ControlPropertyEditorKind Editor = ControlPropertyEditorKind::Auto;
	std::wstring SampleValue;
	struct Choice
	{
		std::wstring DisplayName;
		std::wstring ValueText;
	};
	std::vector<Choice> Choices;
	std::optional<double> Minimum;
	std::optional<double> Maximum;
	std::optional<double> Step;
	ControlPropertyPersistence Persistence = ControlPropertyPersistence::Automatic;
	const BindingPropertyMetadata* Metadata = nullptr;
};

namespace DesignerPropertyCatalog
{
	using TrackedPropertyValues = std::map<std::wstring, DesignerStyleValue>;

	/** Maps runtime property metadata to a Designer-serializable literal kind. */
	bool TryGetStyleValueKind(
		const BindingPropertyMetadata& metadata,
		DesignerStyleValueKind& out);

	/** Returns writable properties whose value types the Designer can persist. */
	std::vector<DesignerPropertyDescriptor> GetStyleProperties(Control& target);

	/** Returns generic PropertyGrid entries after design visibility/persistence filtering. */
	std::vector<DesignerPropertyDescriptor> GetBrowsableProperties(Control& target);

	/**
	 * Returns every Designer-browsable scalar property for the ordinary property
	 * panel. Unlike GetBrowsableProperties, this includes legacy-serialized
	 * properties; transient runtime state remains excluded.
	 */
	std::vector<DesignerPropertyDescriptor> GetPropertyGridProperties(Control& target);

	const DesignerPropertyDescriptor* Find(
		const std::vector<DesignerPropertyDescriptor>& properties,
		const std::wstring& name);

	/**
	 * Converts and coerces a typed literal with the same metadata path used by
	 * Binding and ControlStyleSheet, without mutating the target.
	 */
	bool ValidateStyleValue(
		Control& target,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		std::wstring* outError = nullptr);

	/** Captures the current effective value using the property's canonical name/kind. */
	bool CaptureValue(
		Control& target,
		const std::wstring& propertyName,
		std::wstring* outCanonicalName,
		DesignerStyleValue& out,
		std::wstring* outError = nullptr);

	/** Applies a Local value and returns the post-Coerce canonical representation. */
	bool ApplyValue(
		Control& target,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		std::wstring* outCanonicalName = nullptr,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);

	/** True when a design edit belongs in the generic typed metadata bag. */
	bool UsesMetadataPersistence(const BindingPropertyMetadata& metadata) noexcept;

	/**
	 * Captures the effective value and synchronizes the generic metadata bag
	 * according to the property's persistence metadata. Legacy and transient
	 * properties are deliberately removed from the bag.
	 */
	bool TrackCurrentValue(
		Control& target,
		TrackedPropertyValues& trackedValues,
		const std::wstring& propertyName,
		std::wstring* outCanonicalName = nullptr,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);

	/** Applies a Local value and synchronizes its Designer persistence. */
	bool ApplyAndTrackValue(
		Control& target,
		TrackedPropertyValues& trackedValues,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		std::wstring* outCanonicalName = nullptr,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);

	/** Clears the Local value, exposes the next value source, and untracks it. */
	bool ResetAndUntrackValue(
		Control& target,
		TrackedPropertyValues& trackedValues,
		const std::wstring& propertyName,
		std::wstring* outCanonicalName = nullptr,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);
}
