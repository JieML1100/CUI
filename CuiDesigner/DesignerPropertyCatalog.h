#pragma once

#include "DesignerStyleSheet.h"
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

class ResourceLoadContext;
namespace DesignerModel { struct DesignNode; }

/**
 * Designer-facing projection of one readable runtime property. The runtime
 * metadata remains authoritative; this descriptor adds a literal kind and a
 * representative text value. Metadata decides whether the row is editable.
 */
struct DesignerPropertyDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	std::wstring Category;
	int CategoryOrder = 1000;
	int Order = 0;
	DesignerStyleValueKind ValueKind = DesignerStyleValueKind::String;
	DependencyPropertyEditorKind Editor = DependencyPropertyEditorKind::Auto;
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
	DependencyPropertyPersistence Persistence = DependencyPropertyPersistence::Automatic;
	const DependencyPropertyMetadata* Metadata = nullptr;
};

namespace DesignerPropertyCatalog
{
	using TrackedPropertyValues = std::map<std::wstring, DesignerStyleValue>;

	/** Maps runtime property metadata to a Designer-serializable literal kind. */
	bool TryGetStyleValueKind(
		const DependencyPropertyMetadata& metadata,
		DesignerStyleValueKind& out);

	/** Returns writable properties whose value types the Designer can persist. */
	std::vector<DesignerPropertyDescriptor> GetStyleProperties(Control& target);
	/** Schema-only equivalent; never constructs or reads a Control instance. */
	std::vector<DesignerPropertyDescriptor> GetStyleProperties(
		std::span<const DependencyPropertyMetadata* const> properties);

	/** Resolves one writable persistable property without enumerating the catalog. */
	bool TryGetStyleProperty(
		Control& target,
		const std::wstring& propertyName,
		DesignerPropertyDescriptor& out);
	bool TryGetStyleProperty(
		std::span<const DependencyPropertyMetadata* const> properties,
		const std::wstring& propertyName,
		DesignerPropertyDescriptor& out);

	/** Returns readable observable properties, including read-only transient state. */
	std::vector<DesignerPropertyDescriptor> GetConditionProperties(Control& target);
	std::vector<DesignerPropertyDescriptor> GetConditionProperties(
		std::span<const DependencyPropertyMetadata* const> properties);

	/** Returns generic PropertyGrid entries after design visibility/persistence filtering. */
	std::vector<DesignerPropertyDescriptor> GetBrowsableProperties(Control& target);

	/**
	 * Returns every Designer-browsable scalar property for the ordinary property
	 * panel. Unlike GetBrowsableProperties, this includes native-field
	 * and read-only properties; transient runtime state remains excluded.
	 */
	std::vector<DesignerPropertyDescriptor> GetPropertyGridProperties(Control& target);
	/** Schema-driven property rows for a normalized XAML element node. */
	std::vector<DesignerPropertyDescriptor> GetNodeProperties(UIClass nativeType);

	/** Reads an authored node value, falling back to dependency-property metadata. */
	bool CaptureNodeValue(
		const DesignerModel::DesignNode& node,
		const std::wstring& propertyName,
		DesignerStyleValue& out,
		std::wstring* outCanonicalName = nullptr,
		std::wstring* outError = nullptr);
	/** Converts an authored/default node value into its runtime BindingValue. */
	bool ReadNodeValue(
		const DesignerModel::DesignNode& node,
		const std::wstring& propertyName,
		BindingValue& out,
		std::wstring* outCanonicalName = nullptr,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	/** Installs one canonical local literal and removes a competing Binding. */
	bool ApplyNodeValue(
		DesignerModel::DesignNode& node,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outCanonicalName = nullptr,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	/** Clears the node's local literal/expression and exposes metadata default. */
	bool ResetNodeValue(
		DesignerModel::DesignNode& node,
		const std::wstring& propertyName,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outCanonicalName = nullptr,
		std::wstring* outError = nullptr);

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
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	/**
	 * Schema-only conversion. CoerceValueCallback is intentionally deferred
	 * until a real target receives the value, matching WPF property semantics.
	 */
	bool NormalizeStyleValue(
		const DependencyPropertyMetadata& metadata,
		const DesignerStyleValue& value,
		DesignerStyleValue& outCanonical,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});

	/** Validates a readable/observable metadata value used by Style Trigger. */
	bool ValidateConditionValue(
		Control& target,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});
	bool ValidateConditionValue(
		const DependencyPropertyMetadata& metadata,
		const DesignerStyleValue& value,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {});

	/** Captures the current effective value using the property's canonical name/kind. */
	bool CaptureValue(
		Control& target,
		const std::wstring& propertyName,
		std::wstring* outCanonicalName,
		DesignerStyleValue& out,
		std::wstring* outError = nullptr);
	/** Captures the declared metadata default without constructing a target. */
	bool CaptureDefaultValue(
		const DependencyPropertyMetadata& metadata,
		DesignerStyleValue& out,
		std::wstring* outError = nullptr);

	/** Applies a value-source contribution and returns its effective representation. */
	bool ApplyValue(
		Control& target,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		std::wstring* outCanonicalName = nullptr,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {},
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Local);

	/** True when a design edit belongs in the generic typed metadata bag. */
	bool UsesMetadataPersistence(const DependencyPropertyMetadata& metadata) noexcept;

	/**
	 * Captures the effective value and synchronizes the generic metadata bag
	 * according to the property's persistence metadata. Native and transient
	 * properties are deliberately removed from the bag.
	 */
	bool TrackCurrentValue(
		Control& target,
		TrackedPropertyValues& trackedValues,
		const std::wstring& propertyName,
		std::wstring* outCanonicalName = nullptr,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);

	/** Applies a value-source contribution and synchronizes Designer persistence. */
	bool ApplyAndTrackValue(
		Control& target,
		TrackedPropertyValues& trackedValues,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		std::wstring* outCanonicalName = nullptr,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr,
		const std::wstring& resourceBasePath = {},
		const std::shared_ptr<ResourceLoadContext>& resources = {},
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Local);

	/** Clears the Local value, exposes the next value source, and untracks it. */
	bool ResetAndUntrackValue(
		Control& target,
		TrackedPropertyValues& trackedValues,
		const std::wstring& propertyName,
		std::wstring* outCanonicalName = nullptr,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);
}
