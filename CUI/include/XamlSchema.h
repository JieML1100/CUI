#pragma once

#include "CuiBuildFeatures.h"
#if !CUI_ENABLE_DYNAMIC_XAML
#error XamlSchema is available only in the CUI design-runtime variant
#endif

#include "Binding.h"
#include "Event.h"
#include "RuntimeTypeMetadata.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * Property contract owned by an immutable declarative type descriptor.
 *
 * Unlike DependencyPropertyRegistry entries this definition does not require a
 * C++ owner type or getter/setter pair. DeclarativeTypeDescriptor creates one
 * shared DependencyPropertyMetadata object for the XAML type; Control instances
 * keep only their value slots.
 *
 * Object-valued definitions must provide a non-empty, concrete DefaultValue.
 * The default's runtime type becomes part of the property contract, so values
 * of unrelated object types cannot be assigned through Binding or styles.
 */
struct DeclarativePropertyDefinition
{
	std::wstring Name;
	BindingValueKind ValueKind = BindingValueKind::String;
	BindingValue DefaultValue = BindingValue(std::wstring{});
	/** Optional closed set accepted after conversion; used by declarative enums. */
	std::vector<BindingValue> AllowedValues;
	DependencyPropertyFlags Flags = DependencyPropertyFlags::None;
	/** Concrete trigger used when Binding requests DataSourceUpdateMode::Default. */
	DataSourceUpdateMode DefaultUpdateMode =
		DataSourceUpdateMode::OnPropertyChanged;
	/** Stable identity shared by instances when Flags contains Inherits. */
	std::wstring InheritanceKey;
	DependencyPropertyDesignMetadata Design;
	/** Public XAML/style/Binding writes are rejected; component behavior may update it. */
	bool IsReadOnly = false;
};

enum class DeclarativeContentCardinality : unsigned char
{
	Single,
	Multiple,
};

/** One logical content slot declared by a XAML type. */
struct DeclarativeContentPropertyDefinition final
{
	std::wstring Name;
	std::wstring DisplayName;
	DeclarativeContentCardinality Cardinality =
		DeclarativeContentCardinality::Single;
	bool IsDefault = false;
};

/**
 * Immutable schema shared by every runtime instance of one XAML type.
 *
 * The descriptor owns property/event metadata. Controls attach the descriptor
 * once and allocate only compact per-instance property value slots.
 */
class DeclarativeTypeDescriptor final
{
public:
	static std::shared_ptr<const DeclarativeTypeDescriptor> Create(
		RuntimeTypeId type,
		std::vector<DeclarativePropertyDefinition> properties,
		std::vector<DeclarativeEventDefinition> events = {},
		std::vector<DeclarativeContentPropertyDefinition> contentProperties = {},
		std::wstring* outError = nullptr);

	const RuntimeTypeId& TypeId() const noexcept { return _type; }
	bool IsEquivalentTo(const DeclarativeTypeDescriptor& other) const;

	std::size_t PropertyCount() const noexcept { return _properties.size(); }
	const DependencyPropertyMetadata* FindProperty(
		const std::wstring& propertyName) const noexcept;
	const DependencyPropertyMetadata* FindProperty(
		BindingSourcePropertyToken property) const noexcept;
	std::span<const DependencyPropertyMetadata* const> Properties() const noexcept
	{
		return _propertyMetadata;
	}
	bool TryGetPropertyDefault(
		std::size_t slot,
		BindingValue& value) const;
	bool HasInheritedProperties() const noexcept
	{
		return _hasInheritedProperties;
	}

	const DeclarativeEventDefinition* FindEvent(
		const std::wstring& eventName) const noexcept;
	std::span<const DeclarativeEventDefinition> Events() const noexcept
	{
		return _events;
	}

	const DeclarativeContentPropertyDefinition* FindContentProperty(
		const std::wstring& propertyName) const noexcept;
	std::span<const DeclarativeContentPropertyDefinition>
		ContentProperties() const noexcept
	{
		return _contentProperties;
	}
	const DeclarativeContentPropertyDefinition*
		DefaultContentProperty() const noexcept;

private:
	struct PropertyEntry final
	{
		BindingValue DefaultValue;
		std::vector<BindingValue> AllowedValues;
		std::unique_ptr<DependencyProperty> Property;
		std::unique_ptr<DependencyPropertyMetadata> Metadata;
	};

	explicit DeclarativeTypeDescriptor(RuntimeTypeId type)
		: _type(std::move(type)) {}

	bool Build(
		std::vector<DeclarativePropertyDefinition> properties,
		std::vector<DeclarativeEventDefinition> events,
		std::vector<DeclarativeContentPropertyDefinition> contentProperties,
		std::wstring* outError);

	RuntimeTypeId _type;
	std::vector<PropertyEntry> _properties;
	std::vector<const DependencyPropertyMetadata*> _propertyMetadata;
	std::unordered_map<std::wstring, std::size_t> _propertyIndex;
	std::unordered_map<std::uint64_t, std::size_t> _propertyTokenIndex;
	std::vector<DeclarativeEventDefinition> _events;
	std::unordered_map<std::wstring, std::size_t> _eventIndex;
	std::vector<DeclarativeContentPropertyDefinition> _contentProperties;
	std::unordered_map<std::wstring, std::size_t> _contentPropertyIndex;
	std::size_t _defaultContentProperty = static_cast<std::size_t>(-1);
	bool _hasInheritedProperties = false;
};

/**
 * Per-document-session canonical descriptor table populated by XAML loading.
 * It is infrastructure for materializers, not an application type registry.
 */
class XamlSchemaContext final
{
public:
	std::shared_ptr<const DeclarativeTypeDescriptor> Find(
		const RuntimeTypeId& type) const;
	std::shared_ptr<const DeclarativeTypeDescriptor> GetOrAdd(
		std::shared_ptr<const DeclarativeTypeDescriptor> descriptor,
		std::wstring* outError = nullptr);

private:
	mutable std::mutex _mutex;
	std::unordered_map<RuntimeTypeId,
		std::shared_ptr<const DeclarativeTypeDescriptor>, RuntimeTypeIdHash>
		_types;
};
